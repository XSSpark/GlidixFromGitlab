/*
	Glidix kernel

	Copyright (c) 2021, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <glidix/thread/process.h>
#include <glidix/util/treemap.h>
#include <glidix/util/init.h>
#include <glidix/util/log.h>
#include <glidix/util/panic.h>
#include <glidix/util/memory.h>
#include <glidix/hw/pagetab.h>
#include <glidix/util/string.h>

/**
 * The lock protecting the process table.
 */
static Mutex procTableLock;

/**
 * The process table.
 */
static TreeMap* procTable;

/**
 * The next available PID.
 * 
 * TODO: better allocation system (note that a PID has be different from every other running process'
 * PID, PGID and SID!).
 */
static pid_t procNextPID = 1;

/**
 * The anonymous mapping. This is a special mapping description, with the refcount being initialized
 * to one and hence never released. This single object can be reused for ALL anonymous mappings,
 * as there is no associated file and when we fault we just read out all-zeroes.
 */
static ProcessMapping procAnonMapping = {
	.refcount = 1,
	.inode = NULL,
	.addr = 0,
	.offset = 0,
	.oflags = O_RDWR,
	.mflags = MAP_PRIVATE | MAP_ANON,
};

static void procInit()
{
	kprintf("Initializing the process table...\n");

	procTable = treemapNew();
	if (procTable == NULL)
	{
		panic("Failed to allocate the process table!");
	};
};

KERNEL_INIT_ACTION(procInit, KIA_PROCESS_INIT);

static ProcessMapping* procMappingDup(ProcessMapping *mapping)
{
	__sync_add_and_fetch(&mapping->refcount, 1);
	return mapping;
};

static void procMappingUnref(ProcessMapping *mapping)
{
	if (__sync_add_and_fetch(&mapping->refcount, -1) == 0)
	{
		vfsInodeUnref(mapping->inode);
		kfree(mapping);
	};
};

static void _procUnrefMappingWalk(TreeMap *treemap, uint32_t index, void *value_, void *context_)
{
	ProcessMapping *mapping = (ProcessMapping*) value_;
	procMappingUnref(mapping);
};

static void procDeletePageTableRecur(void *ptr, int depth)
{
	if (depth == 4)
	{
		// `ptr` actually points to a user page
		komUserPageUnref(ptr);
	}
	else
	{
		uint64_t *table = (uint64_t*) ptr;
		int i;
		int limit = depth == 0 ? 256 : 512;
		for (i=0; i<limit; i++)
		{
			uint64_t ent = table[i];
			if (ent & PT_PRESENT)
			{
				void *sub = komPhysToVirt(ent & PT_PHYS_MASK);
				ASSERT(sub != NULL);
				procDeletePageTableRecur(sub, depth+1);
			};
		};

		komReleaseBlock(ptr, KOM_BUCKET_PAGE);
	};
};

static void procDeletePageTable(void *pml4)
{
	Thread *me = schedGetCurrentThread();
	if (me->proc != NULL && me->proc->pagetabVirt == pml4)
	{
		panic("Process is trying to delete its own page table!");
	};

	procDeletePageTableRecur(pml4, 0);
};

void procUnref(Process *proc)
{
	if (__sync_add_and_fetch(&proc->refcount, -1) == 0)
	{
		treemapWalk(proc->mappingTree, _procUnrefMappingWalk, NULL);
		procDeletePageTable(proc->pagetabVirt);
		treemapDestroy(proc->threads);
		treemapDestroy(proc->mappingTree);
		vfsPathWalkerDestroy(&proc->rootDir);
		vfsPathWalkerDestroy(&proc->currentDir);

		int fd;
		for (fd=0; fd<PROC_MAX_OPEN_FILES; fd++)
		{
			File *fp = proc->fileTable[fd].fp;
			if (fp != NULL) vfsClose(fp);
		};

		kfree(proc);
	};
};

static void procStartup(void *context_)
{
	Thread *me = schedGetCurrentThread();
	ProcessStartupInfo *info = (ProcessStartupInfo*) context_;

	me->proc = info->proc;
	pagetabSetCR3(info->proc->cr3);
	schedSetFSBase(info->fsbase);
	me->thid = 1;

	if (treemapSet(info->proc->threads, 1, me) != 0)
	{
		panic("Failed to add the initial thread to the process' thread table, despite having pre-allocated a slot!");
	};

	info->proc->numThreads = 1;
	
	KernelThreadFunc func = info->func;
	void *param = info->param;
	kfree(info);

	func(param);
	panic("can't return from thread func yet!!");
};

/**
 * Get the page table entry for memory address `addr`. Call this only when the pagemap lock is acquired.
 * If the PTE does not yet exist, it will be as unmapped. NULL is returned if the PTE does not exist and
 * we have furthermore ran out of memory.
 */
static PageNodeEntry* _procGetPageTableEntry(user_addr_t addr)
{
	PageNodeEntry *nodes[4];
	pagetabGetNodes((void*)addr, nodes);

	int i;
	for (i=0; i<3; i++)
	{
		PageNodeEntry *node = nodes[i];
		if (node->value == 0)
		{
			void *nextLevel = komAllocBlock(KOM_BUCKET_PAGE, KOM_POOLBIT_ALL);
			if (nextLevel == NULL)
			{
				return NULL;
			};

			memZeroPage(nextLevel);

			// all intermediate levels are mapped as WRITE, USER, PRESENT, and with NOEXEC,
			// so that we can set these per-page without worrying about the higher levels
			node->value = pagetabGetPhys(nextLevel) | PT_WRITE | PT_USER | PT_PRESENT;

			// invalidate the next node to apply the above
			invlpg(nodes[i+1]);
		};
	};

	return nodes[3];
};

/**
 * Gets a useable pointer to the PTE for `addr` in a different address space. This is only used while
 * cloning page tables. Returns NULL if we ran out of memory trying to allocate paging structures.
 */
static PageNodeEntry* _procGetForeignPageTableEntry(void *pml4, user_addr_t addr)
{
	int indexes[4];
	indexes[3] = (addr >> (12)) & 0x1FF;
	indexes[2] = (addr >> (12+9)) & 0x1FF;
	indexes[1] = (addr >> (12+9+9)) & 0x1FF;
	indexes[0] = (addr >> (12+9+9+9)) & 0x1FF;

	void *table = pml4;

	int i;
	for (i=0; i<4; i++)
	{
		PageNodeEntry *ent = (PageNodeEntry*) table + indexes[i];

		// if we are at the final level, just return it (it's the PTE)
		if (i == 3) return ent;

		if (ent->value == 0)
		{
			void *nextLevel = komAllocBlock(KOM_BUCKET_PAGE, KOM_POOLBIT_ALL);
			if (nextLevel == NULL)
			{
				return NULL;
			};

			memZeroPage(nextLevel);

			// all intermediate levels are mapped as WRITE, USER, PRESENT, and with NOEXEC,
			// so that we can set these per-page without worrying about the higher levels
			ent->value = pagetabGetPhys(nextLevel) | PT_WRITE | PT_USER | PT_PRESENT;	

			// go to it
			table = nextLevel;
		}
		else
		{
			table = komPhysToVirt(ent->value & PT_PHYS_MASK);
			ASSERT(table != NULL);
		};
	};

	// should never get here
	return NULL;
};

static void _procPageCloneWalkCallback(TreeMap *parentTree, uint32_t index, void *value_, void *context_)
{
	PageCloneContext *ctx = (PageCloneContext*) context_;
	ProcessMapping *mapping = (ProcessMapping*) value_;
	user_addr_t addr = ((user_addr_t) index) << 12;
	PageNodeEntry *parentPTE = _procGetPageTableEntry(addr);
	PageNodeEntry *childPTE = _procGetForeignPageTableEntry(ctx->childPageTable, addr);
	
	if (parentPTE == NULL || childPTE == NULL)
	{
		ctx->err = ENOMEM;
		return;
	};

	// copy the mapping into the new table
	if (treemapSet(ctx->childTree, index, mapping) != 0)
	{
		ctx->err = ENOMEM;
		return;
	};

	// successful, increment refcount
	procMappingDup(mapping);

	// if this is a private mapping, and we have PROT_WRITE permission, and the page is mapped writeable,
	// we must turn it into copy-on-write
	if ((mapping->mflags & MAP_PRIVATE) && (parentPTE->value & PT_PRESENT)
		&& (parentPTE->value & PT_PROT_WRITE))
	{
		// mark non-writeable, copy-on-write for parent
		parentPTE->value &= ~(PT_WRITE);
		parentPTE->value |= PT_COW;

		// inform any other CPUs running this process that this happened
		invlpg((void*) addr);
		cpuInvalidatePage(ctx->parent->cr3, (void*) addr);
	};

	// if the page is present, increase its refcount
	if (parentPTE->value & PT_PRESENT)
	{
		void *page = komPhysToVirt(parentPTE->value & PT_PHYS_MASK);
		ASSERT(page != NULL);
		komUserPageDup(page);
	};

	// map into the new table
	childPTE->value = parentPTE->value;
};

pid_t procCreate(KernelThreadFunc func, void *param)
{
	Thread *me = schedGetCurrentThread();

	// create the startup info struct
	ProcessStartupInfo *info = (ProcessStartupInfo*) kmalloc(sizeof(ProcessStartupInfo));
	if (info == NULL)
	{
		return -ENOMEM;
	};

	info->fsbase = me->fsbase;
	info->func = func;
	info->param = param;

	// allocate the process struct
	Process *child = (Process*) kmalloc(sizeof(Process));
	if (child == NULL)
	{
		kfree(info);
		return -ENOMEM;
	};

	info->proc = child;

	// allocate the thread table
	TreeMap *threads = treemapNew();
	if (threads == NULL)
	{
		kfree(child);
		kfree(info);
		return -ENOMEM;
	};

	// allocate the mapping tree
	TreeMap *mappingTree = treemapNew();
	if (mappingTree == NULL)
	{
		treemapDestroy(threads);
		kfree(child);
		kfree(info);
		return -ENOMEM;
	};

	// pre-allocate memory for the '1' entry
	if (treemapSet(threads, 1, NULL) != 0)
	{
		treemapDestroy(mappingTree);
		treemapDestroy(threads);
		kfree(child);
		kfree(info);
		return -ENOMEM;
	};

	// create the PML4, copy the 509 and 510 entries from our current way, as
	// these contain the userspace auxiliary area, and the kernel space, respectively;
	// then make 511 point back to itself for recursive mapping
	uint64_t *myPML4 = (uint64_t*) 0xFFFFFFFFFFFFF000;
	uint64_t *newPML4 = (uint64_t*) komAllocBlock(KOM_BUCKET_PAGE, KOM_POOLBIT_ALL);

	if (newPML4 == NULL)
	{
		treemapDestroy(mappingTree);
		treemapDestroy(threads);
		kfree(child);
		kfree(info);
		return -ENOMEM;
	};

	memZeroPage(newPML4);

	newPML4[509] = myPML4[509];
	newPML4[510] = myPML4[510];
	newPML4[511] = pagetabGetPhys(myPML4) | PT_PRESENT | PT_WRITE | PT_NOEXEC;

	// fill out the process structure
	memset(child, 0, sizeof(Process));
	child->cr3 = pagetabGetPhys(myPML4);
	child->pagetabVirt = newPML4;
	child->mappingTree = mappingTree;
	child->parent = me->proc == NULL ? 1 : me->proc->pid;

	child->rootDir = vfsPathWalkerGetRoot();
	child->currentDir = vfsPathWalkerGetCurrentDir();
	child->threads = threads;
	child->refcount = 1;

	// if the calling process is non-kernel, inherit properties
	if (me->proc != NULL)
	{
		child->euid = me->proc->euid;
		child->suid = me->proc->suid;
		child->ruid = me->proc->ruid;

		child->egid = me->proc->egid;
		child->sgid = me->proc->sgid;
		child->rgid = me->proc->rgid;

		child->sid = me->proc->sid;
		child->pgid = me->proc->pgid;

		PageCloneContext ctx;
		ctx.parent = me->proc;
		ctx.childPageTable = newPML4;
		ctx.childTree = mappingTree;
		ctx.err = 0;

		mutexLock(&me->proc->mapLock);
		treemapWalk(me->proc->mappingTree, _procPageCloneWalkCallback, &ctx);
		mutexUnlock(&me->proc->mapLock);

		if (ctx.err != 0)
		{
			procUnref(child);
			kfree(info);
			return -ctx.err;
		};

		mutexLock(&me->proc->fileTableLock);
		int fd;
		for (fd=0; fd<PROC_MAX_OPEN_FILES; fd++)
		{
			FileTableEntry *oldEnt = &me->proc->fileTable[fd];
			FileTableEntry *newEnt = &child->fileTable[fd];

			if (oldEnt->fp != NULL && oldEnt->fp != PROC_FILE_RESV)
			{
				File *newFP = vfsFork(oldEnt->fp);
				if (newFP == NULL)
				{
					mutexUnlock(&me->proc->fileTableLock);
					procUnref(child);
					kfree(info);
					return -ENOMEM;
				};

				newEnt->fp = newFP;
				newEnt->cloexec = oldEnt->cloexec;
			};
		};
		mutexUnlock(&me->proc->fileTableLock);
	};

	// try allocating a PID
	mutexLock(&procTableLock);

	pid_t pid = __sync_fetch_and_add(&procNextPID, 1);

	// map that PID to us
	child->pid = pid;
	if (treemapSet(procTable, pid, child) != 0)
	{
		mutexUnlock(&procTableLock);
		procUnref(child);
		kfree(info);
		return -ENOMEM;
	};

	// now try creating the thread
	Thread *startupThread = schedCreateKernelThread(procStartup, info, NULL);
	if (startupThread == NULL)
	{
		// remove us from the table
		treemapSet(procTable, pid, NULL);
		mutexUnlock(&procTableLock);
		procUnref(child);
		kfree(info);
	};

	// the new thread takes ownership of 'info'
	mutexUnlock(&procTableLock);
	schedDetachKernelThread(startupThread);
	return pid;
};

user_addr_t procMap(user_addr_t addr, size_t length, int prot, int flags, File *fp, off_t offset, errno_t *err)
{
	if (addr == 0 && (flags & MAP_FIXED) == 0)
	{
		// TODO: allocate address space automatically!
		if (err != NULL) *err = EOPNOTSUPP;
		return MAP_FAILED;
	};

	if ((addr & 0xFFF) || (offset & 0xFFF) || length > PROC_USER_ADDR_MAX || addr > PROC_USER_ADDR_MAX
		|| (addr+length) > PROC_USER_ADDR_MAX || length == 0)
	{
		// not page-aligned, or not within range
		if (err != NULL) *err = EINVAL;
		return MAP_FAILED;
	};

	if ((prot & PROT_ALL) != prot)
	{
		// invalid prot set
		if (err != NULL) *err = EINVAL;
		return MAP_FAILED;
	};

	if ((flags & MAP_ALLFLAGS) != flags)
	{
		// invalid flags set
		if (err != NULL) *err = EINVAL;
		return MAP_FAILED;
	};

	int mappingType = flags & (MAP_PRIVATE | MAP_SHARED);
	if (mappingType != MAP_PRIVATE && mappingType != MAP_SHARED)
	{
		// exactly one of MAP_PRIVATE or MAP_SHARED must be set in flags, but isn't
		if (err != NULL) *err = EINVAL;
		return MAP_FAILED;
	};

	if ((flags & MAP_ANON && fp != NULL) || ((flags & MAP_ANON) == 0 && fp == NULL))
	{
		// with anonymous mapings, the file pointer must be NULL, and with non-anonymous
		// mappings, it must not be NULL
		if (err != NULL) *err = EBADF;
		return MAP_FAILED;
	};

	if (fp != NULL)
	{
		if ((fp->walker.current->mode & VFS_MODE_TYPEMASK) != VFS_MODE_REGULAR)
		{
			// mapping something other than a regular file
			if (err != NULL) *err = EACCES;
			return MAP_FAILED;
		};

		if ((fp->oflags & O_RDONLY) == 0)
		{
			// file is not open for reading
			if (err != NULL) *err = EACCES;
			return MAP_FAILED;
		};

		if (mappingType == MAP_SHARED && (prot & PROT_WRITE) && (fp->oflags & O_WRONLY) == 0)
		{
			// requesting shared write access but file is not open for writing
			if (err != NULL) *err = EACCES;
			return MAP_FAILED;
		};
	};

	// get the current process
	Process *proc = schedGetCurrentThread()->proc;

	// figure out what mapping to use; for anonymous, we use the global 'anonymous' mapping as
	// it's identical in all contexts, otherwise allocate our own mapping description
	ProcessMapping *mapping;
	if (fp == NULL)
	{
		mapping = procMappingDup(&procAnonMapping);
	}
	else
	{
		mapping = (ProcessMapping*) kmalloc(sizeof(ProcessMapping));
		if (mapping == NULL)
		{
			if (err != NULL) *err = ENOMEM;
			return MAP_FAILED;
		};

		mapping->refcount = 1;
		mapping->addr = addr;
		mapping->offset = offset;
		mapping->oflags = fp->oflags;
		mapping->inode = vfsInodeDup(fp->walker.current);
		mapping->mflags = flags;
	};

	// now we map
	user_addr_t scan;
	errno_t status = 0;

	mutexLock(&proc->mapLock);
	for (scan=addr; scan<addr+length; scan+=PAGE_SIZE)
	{
		// get the page and fail with ENOMEM if we can't
		PageNodeEntry *pte = _procGetPageTableEntry(scan);
		if (pte == NULL)
		{
			status = ENOMEM;
			break;
		};

		if (pte->value & PT_PRESENT)
		{
			void *page = komPhysToVirt(pte->value & PT_PHYS_MASK);
			ASSERT(page != NULL);

			pte->value = 0;

			// inform other CPUs that the page was unmapped
			invlpg((void*) scan);
			cpuInvalidatePage(proc->cr3, (void*) scan);

			komUserPageUnref(page);
		};

		// specify the access bits
		if (prot & PROT_READ) pte->value |= PT_PROT_READ;
		if (prot & PROT_WRITE) pte->value |= PT_PROT_WRITE;
		if (prot & PROT_EXEC) pte->value |= PT_PROT_EXEC;

		// try to map into the page mapping tree
		uint32_t pageIndex = (scan >> 12);
		ProcessMapping *old = (ProcessMapping*) treemapGet(proc->mappingTree, pageIndex);
		status = treemapSet(proc->mappingTree, pageIndex, mapping);
		if (status != 0) break;

		// upref the new mapping and downref the old
		procMappingDup(mapping);
		if (old != NULL) procMappingUnref(old);
	};
	mutexUnlock(&proc->mapLock);

	// get rid of our initial reference
	procMappingUnref(mapping);

	if (status != 0)
	{
		// an error occured
		if (err != NULL) *err = status;
		return MAP_FAILED;
	};

	return addr;
};

static void _procUnmapWalkCallback(TreeMap *mappingTree, uint32_t pageIndex, void *value, void *context)
{
	Process *proc = (Process*) context;
	ProcessMapping *mapping = (ProcessMapping*) value;

	// remove from the mapping tree
	treemapSet(mappingTree, pageIndex, NULL);

	// unref it
	procMappingUnref(mapping);

	// if the PTE is mapped, unmap it, unref the page, invalidate
	user_addr_t userAddr = ((user_addr_t) pageIndex) << 12;
	PageNodeEntry *pte = _procGetPageTableEntry(userAddr);

	if (pte->value & PT_PRESENT)
	{
		void *canon = komPhysToVirt(pte->value & PT_PHYS_MASK);
		ASSERT(canon != NULL);

		pte->value = 0;
		invlpg((void*) userAddr);
		cpuInvalidatePage(proc->cr3, (void*) userAddr);
		
		komUserPageUnref(canon);
	};
};

void procBeginExec()
{
	Process *proc = schedGetCurrentThread()->proc;

	if (proc->numThreads != 1)
	{
		panic("TODO: i don't know how to kill other threads yet!");
	};

	// reset signal dispositions
	schedResetSigActions();

	// close all cloexec files
	mutexLock(&proc->fileTableLock);
	int fd;
	for (fd=0; fd<PROC_MAX_OPEN_FILES; fd++)
	{
		FileTableEntry *ent = &proc->fileTable[fd];
		if (ent->cloexec && ent->fp != NULL)
		{
			vfsClose(ent->fp);
			ent->fp = NULL;
			ent->cloexec = 0;
		};
	};
	mutexUnlock(&proc->fileTableLock);
	
	// unmap all userspace pages
	mutexLock(&proc->mapLock);
	treemapWalk(proc->mappingTree, _procUnmapWalkCallback, proc);
	mutexUnlock(&proc->mapLock);
};

/**
 * Fill out `signfo` with error details, and return -1. Helper function for reporting page fault errors.
 */
static int _procPageFaultInvalid(Process *proc, user_addr_t addr, ksiginfo_t *siginfo, int signo, int code)
{
	if (siginfo != NULL)
	{
		memset(siginfo, 0, sizeof(ksiginfo_t));
		siginfo->si_signo = signo;
		siginfo->si_code = code;
		siginfo->si_pid = proc->pid;
		siginfo->si_uid = proc->ruid;
		siginfo->si_addr = (void*) addr;
	};

	return -1;
};

static int _procPageFault(user_addr_t addr, int faultFlags, ksiginfo_t *siginfo)
{
	Process *proc = schedGetCurrentThread()->proc;

	// find check if the address is even within the range allowed for userspace
	if (addr >= PROC_USER_ADDR_MAX)
	{
		return _procPageFaultInvalid(proc, addr, siginfo, SIGSEGV, SEGV_MAPERR);
	};

	// get the mapping at that location
	uint32_t pageIndex = addr >> 12;
	ProcessMapping *mapping = (ProcessMapping*) treemapGet(proc->mappingTree, pageIndex);
	if (mapping == NULL)
	{
		// no mapping at this address!
		return _procPageFaultInvalid(proc, addr, siginfo, SIGSEGV, SEGV_MAPERR);
	};

	// mapping exists, now get the page itself
	PageNodeEntry *pte = _procGetPageTableEntry(addr);

	// check if we have the required permissions
	uint64_t requiredPerms = PT_PROT_READ;
	if (faultFlags & PF_WRITE) requiredPerms |= PT_PROT_WRITE;
	if (faultFlags & PF_FETCH) requiredPerms |= PT_PROT_EXEC;

	uint64_t permsSet = pte->value & PT_PROT_MASK;
	if ((permsSet & requiredPerms) != requiredPerms)
	{
		// not all permissions were granted
		return _procPageFaultInvalid(proc, addr, siginfo, SIGSEGV, SEGV_ACCERR);
	};

	// if it's not yet called into memory, call it in now
	if ((pte->value & PT_PRESENT) == 0)
	{
		off_t offset = (mapping->offset + addr - mapping->addr) & ~0xFFFUL;
		void *page = vfsInodeGetPage(mapping->inode, offset);

		if (page == NULL)
		{
			// we couldn't get the page!
			return _procPageFaultInvalid(proc, addr, siginfo, SIGBUS, BUS_OBJERR);
		};

		uint64_t newPTE = pagetabGetPhys(page) | PT_PRESENT | PT_USER | permsSet;
		if (mapping->inode == NULL)
		{
			// anonymous mapping, so allow writing to it even whether private or shared,
			// if we have write permission; we don't need to copy-on-write as this is
			// already a new, clear page
			if (permsSet & PT_PROT_WRITE)
			{
				newPTE |= PT_WRITE;
			};
		}
		else if (mapping->mflags & MAP_SHARED)
		{
			// shared mapping, so if we have write permission, allow code to write to
			// this page directly
			if (permsSet & PT_PROT_WRITE)
			{
				newPTE |= PT_WRITE;
			};
		}
		else
		{
			// private mapping, so if we have write permission, mark it copy-on-write
			if (permsSet & PT_PROT_WRITE)
			{
				newPTE |= PT_COW;
			};
		};

		// if we don't have execute permission, mark it non-exec
		if ((permsSet & PT_PROT_EXEC) == 0)
		{
			newPTE |= PT_NOEXEC;
		};

		// set it
		pte->value = newPTE;
	};

	// if we are trying to write, and the page is copy-on-write, copy it
	if ((faultFlags & PF_WRITE) && (pte->value & PT_COW))
	{
		void *oldPage = komPhysToVirt(pte->value & PT_PHYS_MASK);
		ASSERT(oldPage != NULL);

		void *newPage = komAllocUserPage();
		if (newPage == NULL)
		{
			// out of memory!
			return _procPageFaultInvalid(proc, addr, siginfo, SIGBUS, BUS_ADRERR);
		};

		// copy to the new page
		memcpy(newPage, oldPage, PAGE_SIZE);

		// create the new, writeable PTE, mark it no-exec if we don't hav exec permission
		uint64_t newPTE = pagetabGetPhys(newPage) | PT_PRESENT | PT_USER | PT_WRITE | permsSet;
		if ((permsSet & PT_PROT_EXEC) == 0)
		{
			newPTE |= PT_NOEXEC;
		};

		// set it
		pte->value = newPTE;

		// inform other CPUs about this before we release the page
		cpuInvalidatePage(proc->cr3, (void*) addr);

		// now release the old page
		komUserPageUnref(oldPage);
	};

	// invalidate the page. we don't have to inform other CPUs; in the worst case, they'll simply page
	// fault too, go through this code and change nothing and invalidate it
	invlpg((void*) addr);
	return 0;
};

int procPageFault(user_addr_t addr, int faultFlags, ksiginfo_t *siginfo)
{
	Process *proc = schedGetCurrentThread()->proc;

	mutexLock(&proc->mapLock);
	int result = _procPageFault(addr, faultFlags, siginfo);
	mutexUnlock(&proc->mapLock);

	return result;
};

int procToKernelCopy(void *ptr, user_addr_t addr, size_t size)
{
	Process *proc = schedGetCurrentThread()->proc;
	char *put = (char*) ptr;

	int status = 0;

	mutexLock(&proc->mapLock);
	while (size != 0)
	{
		if (_procPageFault(addr, 0, NULL) != 0)
		{
			status = -EFAULT;
			break;
		};

		size_t pageLeft = 0x1000 - (addr & 0xFFF);
		size_t sizeToCopy = size > pageLeft ? pageLeft : size;

		memcpy(put, (void*)addr, sizeToCopy);

		addr += sizeToCopy;
		put += sizeToCopy;
		size -= sizeToCopy;
	};
	mutexUnlock(&proc->mapLock);

	return status;
};

int procReadUserString(char *buffer, user_addr_t addr)
{
	Process *proc = schedGetCurrentThread()->proc;
	char *put = buffer;

	int status = 0;
	size_t size = PROC_USER_STRING_SIZE;

	mutexLock(&proc->mapLock);
	while (size != 0)
	{
		if (_procPageFault(addr, 0, NULL) != 0)
		{
			status = -EFAULT;
			break;
		};

		size_t pageLeft = 0x1000 - (addr & 0xFFF);
		size_t sizeToCopy = size > pageLeft ? pageLeft : size;

		memcpy(put, (void*)addr, sizeToCopy);

		size_t i;
		for (i=0; i<sizeToCopy; i++)
		{
			if (put[i] == 0)
			{
				break;
			};
		};

		if (i != sizeToCopy)
		{
			break;
		};

		addr += sizeToCopy;
		put += sizeToCopy;
		size -= sizeToCopy;
	};
	mutexUnlock(&proc->mapLock);

	if (size == 0)
	{
		status = -EOVERFLOW;
	};

	return status;
};

int procToUserCopy(user_addr_t addr, const void *ptr, size_t size)
{
	Process *proc = schedGetCurrentThread()->proc;
	const char *scan = (const char*) ptr;

	int status = 0;

	mutexLock(&proc->mapLock);
	while (size != 0)
	{
		if (_procPageFault(addr, PF_WRITE, NULL) != 0)
		{
			status = -EFAULT;
			break;
		};

		size_t pageLeft = 0x1000 - (addr & 0xFFF);
		size_t sizeToCopy = size > pageLeft ? pageLeft : size;

		memcpy((void*)addr, scan, sizeToCopy);

		addr += sizeToCopy;
		scan += sizeToCopy;
		size -= sizeToCopy;
	};
	mutexUnlock(&proc->mapLock);

	return status;
};

Process* procByPID(pid_t pid)
{
	mutexLock(&procTableLock);
	Process *proc = (Process*) treemapGet(procTable, pid);
	if (proc != NULL) procDup(proc);
	mutexUnlock(&procTableLock);

	return proc;
};

Process* procDup(Process *proc)
{
	__sync_add_and_fetch(&proc->refcount, 1);
	return proc;
};

File* procFileGet(int fd)
{
	if (fd < 0 || fd >= PROC_MAX_OPEN_FILES)
	{
		return NULL;
	};

	Process *me = schedGetCurrentThread()->proc;
	mutexLock(&me->fileTableLock);

	File *fp = me->fileTable[fd].fp;
	if (fp != NULL && fp != PROC_FILE_RESV) vfsDup(fp);

	mutexUnlock(&me->fileTableLock);

	if (fp == PROC_FILE_RESV) return NULL;
	else return fp;
};

int procFileResv()
{
	Process *me = schedGetCurrentThread()->proc;
	mutexLock(&me->fileTableLock);

	int fd;
	for (fd=0; fd<PROC_MAX_OPEN_FILES; fd++)
	{
		FileTableEntry *ent = &me->fileTable[fd];
		if (ent->fp == NULL)
		{
			ent->fp = PROC_FILE_RESV;
			break;
		};
	};

	mutexUnlock(&me->fileTableLock);

	if (fd == PROC_MAX_OPEN_FILES) return -1;
	else return fd;
};

void procFileSet(int fd, File *fp, int cloexec)
{
	Process *me = schedGetCurrentThread()->proc;
	mutexLock(&me->fileTableLock);
	
	FileTableEntry *ent = &me->fileTable[fd];
	if (ent->fp != PROC_FILE_RESV)
	{
		panic("procFileSet() called on a non-reserved descriptor! This is an internal bug!");
	};

	ent->fp = vfsDup(fp);
	ent->cloexec = cloexec;
	
	mutexUnlock(&me->fileTableLock);
};

int procFileClose(int fd)
{
	Process *me = schedGetCurrentThread()->proc;
	mutexLock(&me->fileTableLock);

	FileTableEntry *ent = &me->fileTable[fd];
	File *old = ent->fp;

	ent->fp = NULL;
	ent->cloexec = 0;
	mutexUnlock(&me->fileTableLock);

	if (old == NULL)
	{
		return -EBADF;
	};

	return 0;
};

/**
 * This is called from `procExit()` to walk through the process table, `context_` being the process
 * exiting, and any process who has the exiting process as a parent, will have the parent PID set to
 * 1 (init).
 */
static void _procOrphaneChildrenCallback(TreeMap *tm, uint32_t index, void *value_, void *context_)
{
	Process *parent = (Process*) context_;
	Process *child = (Process*) value_;

	if (child->parent == parent->pid)
	{
		child->parent = 1;
	};
};

noreturn void procExit(int wstatus)
{
	Thread *thread = schedGetCurrentThread();
	Process *proc = thread->proc;

	ksiginfo_t si;
	memset(&si, 0, sizeof(ksiginfo_t));
	si.si_signo = SIGCHLD;
	si.si_pid = proc->pid;
	si.si_uid = proc->ruid;
	si.si_code = CLD_EXITED;

	if (proc->pid == 1)
	{
		panic("The init process attempted to exit!");
	};

	if (proc->numThreads > 1)
	{
		panic("TODO: i don't know how to kill other threads yet!");
	};

	// close all file descriptors
	int fd;
	for (fd=0; fd<PROC_MAX_OPEN_FILES; fd++)
	{
		procFileClose(fd);
	};

	// remove the calling thread (us) from the process' thread table
	mutexLock(&proc->threadTableLock);
	treemapSet(proc->threads, thread->thid, NULL);
	mutexUnlock(&proc->threadTableLock);

	// detach us from the process, and ensure that we don't continue using the page table
	cli();
	thread->proc = NULL;
	pagetabSetCR3(cpuGetCurrent()->kernelCR3);
	sti();

	// with the process table lock held, orphane the children and inform the parent about us
	mutexLock(&procTableLock);
	proc->wstatus = wstatus;
	proc->terminated = 1;
	treemapWalk(procTable, _procOrphaneChildrenCallback, proc);
	Process *parent = (Process*) treemapGet(procTable, proc->parent);
	ASSERT(parent != NULL);

	// if the parent explicitly ignores SIGCHLD or has set SA_NOCLDWAIT on its signal
	// disposition, we inform init (pid 1) instead
	SigAction *sigchldAction = &parent->sigActions[SIGCHLD];
	if (sigchldAction->sa_sigaction_handler == SIG_IGN || sigchldAction->sa_flags & SA_NOCLDWAIT)
	{
		parent = treemapGet(procTable, 1);
		ASSERT(parent != NULL);
	};

	if (parent->childWaiter != NULL) schedWake(parent->childWaiter);
	schedDeliverSignalToProc(parent, &si);
	mutexUnlock(&procTableLock);

	// exit from this thread
	schedDetachKernelThread(thread);
	schedExitThread();
};

static void _procWaitWalkCallback(TreeMap *tm, uint32_t ignore_, void *value_, void *context_)
{
	Process *proc = (Process*) value_;
	ProcWaitContext *ctx = (ProcWaitContext*) context_;

	if (ctx->result > 0)
	{
		return;
	};

	if (proc->parent == ctx->parent)
	{
		if (proc->pid == ctx->pid || ctx->pid == -1 || proc->pgid == -ctx->pid
			|| (ctx->pid == 0 && proc->pgid == ctx->parentPGID))
		{
			if (ctx->result == -ECHILD)
			{
				ctx->result = 0;
			};

			if (proc->terminated)
			{
				ctx->wstatus = proc->wstatus;
				ctx->result = proc->pid;
				ctx->child = proc;
				treemapSet(tm, proc->pid, NULL);
			};
		};
	};
};

pid_t procWait(pid_t pid, int *wstatus, int flags)
{
	if ((flags & PROC_WALL) != flags)
	{
		// invalid flags were set
		return -EINVAL;
	};

	Thread *thread = schedGetCurrentThread();
	Process *proc = thread->proc;

	ProcWaitContext ctx;
	ctx.pid = pid;
	ctx.parent = proc->pid;
	ctx.child = NULL;
	ctx.wstatus = 0;
	ctx.parentPGID = proc->pgid;

	mutexLock(&procTableLock);
	while (1)
	{
		if (proc->childWaiter == NULL)
		{
			proc->childWaiter = thread;
		};

		ctx.result = -ECHILD;
		treemapWalk(procTable, _procWaitWalkCallback, &ctx);
		
		if (ctx.result != 0)
		{
			break;
		};

		if (flags & PROC_WNOHANG)
		{
			break;
		};

		mutexUnlock(&procTableLock);
		schedSuspend();

		mutexLock(&procTableLock);
		if (schedHaveReadySigs())
		{
			ctx.result = -EINTR;
			break;
		};
	};

	if (proc->childWaiter == thread)
	{
		proc->childWaiter = NULL;
	};

	mutexUnlock(&procTableLock);
	if (ctx.child != NULL) procUnref(ctx.child);
	if (wstatus != NULL) *wstatus = ctx.wstatus;
	return ctx.result;
};

static void _procWakeThreadsWalkCallback(TreeMap *map, uint32_t thid, void *th_, void *context_)
{
	Thread *thread = (Thread*) th_;
	schedWake(thread);
};

void procWakeThreads(Process *proc)
{
	mutexLock(&proc->threadTableLock);
	treemapWalk(proc->threads, _procWakeThreadsWalkCallback, NULL);
	mutexUnlock(&proc->threadTableLock);
};

static void _procFindGroupWalkCallback(TreeMap *tm, uint32_t pid, void *value_, void *context_)
{
	int *resultOut = (int*) context_;
	Process *proc = (Process*) value_;

	if (proc->pgid == schedGetCurrentThread()->proc->pid)
	{
		*resultOut = 1;
	};
};

int procSetSessionID()
{
	Process *me = schedGetCurrentThread()->proc;
	if (me->pid == 1)
	{
		// init is not allowed to have a session
		return -EPERM;
	};

	mutexLock(&procTableLock);
	
	// see if any process belongs to the group of which we are the leader
	int foundConflict = 0;
	treemapWalk(procTable, _procFindGroupWalkCallback, &foundConflict);

	if (foundConflict)
	{
		// not allowed
		mutexUnlock(&procTableLock);
		return -EPERM;
	};

	// we can do it
	me->sid = me->pgid = me->pid;

	mutexUnlock(&procTableLock);
	return 0;
};

static void _procGetSessionWalkCallback(TreeMap *treemap, uint32_t pid, void *value_, void *context_)
{
	Process *proc = (Process*) value_;
	ProcessGroupSessionWalkContext *ctx = (ProcessGroupSessionWalkContext*) context_;

	if (proc->pgid == ctx->pgid)
	{
		ctx->sid = proc->sid;
	};
};

int procSetProcessGroup(pid_t pid, pid_t pgid)
{
	if (pid < 0)
	{
		return -EINVAL;
	};

	if (pid == 0)
	{
		pid = schedGetCurrentThread()->proc->pid;
	};

	if (pid == 1)
	{
		// cannot change the process group of init
		return -EPERM;
	};

	if (pgid < 0)
	{
		return -EINVAL;
	};

	if (pgid == 0)
	{
		pgid = pid;
	};

	mutexLock(&procTableLock);

	ProcessGroupSessionWalkContext ctx;
	ctx.pgid = pgid;
	ctx.sid = 0;

	Process *target = treemapGet(procTable, pid);
	pid_t myPID = schedGetCurrentThread()->proc->pid;
	if (target == NULL || (target->pid != myPID && target->parent != myPID))
	{
		mutexUnlock(&procTableLock);
		return -ESRCH;
	};

	treemapWalk(procTable, _procGetSessionWalkCallback, &ctx);
	if (ctx.sid != target->sid || target->pid == target->sid)
	{
		mutexUnlock(&procTableLock);
		return -EPERM;
	};

	target->pgid = pgid;
	mutexUnlock(&procTableLock);

	return 0;
};

static void _procKillWalkCallback(TreeMap *tm, uint32_t ignore_, void *value_, void *context_)
{
	Process *proc = (Process*) value_;
	KillWalkContext *ctx = (KillWalkContext*) context_;
	Process *me = schedGetCurrentThread()->proc;

	if (proc->pid == ctx->pid || ctx->pid == -1 || proc->pgid == -ctx->pid
		|| (ctx->pid == 0 && proc->pgid == me->pgid))
	{
		int permitted = me->euid == 0 || me->ruid == 0 || me->ruid == proc->ruid || me->euid == proc->ruid;
		if (ctx->status == -ESRCH)
		{
			ctx->status = permitted ? 0 : -EPERM;
		};

		if (permitted)
		{
			ksiginfo_t si;
			memset(&si, 0, sizeof(ksiginfo_t));

			si.si_signo = ctx->signo;
			si.si_code = SI_USER;
			si.si_pid = me->pid;
			si.si_uid = me->ruid;

			schedDeliverSignalToProc(proc, &si);
		};
	};
};

int procKill(pid_t pid, int signo)
{
	if (signo < 1 || signo >= SIG_NUM)
	{
		return -EINVAL;
	};

	mutexLock(&procTableLock);

	KillWalkContext ctx;
	ctx.pid = pid;
	ctx.signo = signo;
	ctx.status = -ESRCH;

	treemapWalk(procTable, _procKillWalkCallback, &ctx);

	mutexUnlock(&procTableLock);
	return ctx.status;
};