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

void procUnref(Process *proc)
{
	if (__sync_add_and_fetch(&proc->refcount, -1) == 0)
	{
		// TODO: delete all the pages
		komReleaseBlock(proc->pagetabVirt, KOM_BUCKET_PAGE);
		treemapDestroy(proc->threads);
		treemapDestroy(proc->mappingTree);
		vfsPathWalkerDestroy(&proc->rootDir);
		vfsPathWalkerDestroy(&proc->currentDir);
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

	// TODO: if the calling process is non-kernel, clone the page table
	if (me->proc != NULL)
	{
		panic("I don't know how to clone page tables yet!");
	};

	// fill out the process structure
	memset(child, 0, sizeof(Process));
	child->cr3 = pagetabGetPhys(myPML4);
	child->pagetabVirt = newPML4;
	child->mappingTree = mappingTree;
	child->parent = me->proc == NULL ? 1 : me->proc->pid;
	
	if (me->proc != NULL)
	{
		child->euid = me->proc->euid;
		child->suid = me->proc->suid;
		child->ruid = me->proc->ruid;

		child->egid = me->proc->egid;
		child->sgid = me->proc->sgid;
		child->rgid = me->proc->rgid;
	};

	child->rootDir = vfsPathWalkerGetRoot();
	child->currentDir = vfsPathWalkerGetCurrentDir();
	child->threads = threads;
	child->refcount = 1;

	// try allocating a PID
	mutexLock(&procTableLock);

	pid_t pid;
	for (pid=1; pid<PROC_MAX; pid++)
	{
		if (treemapGet(procTable, pid) == NULL)
		{
			break;
		};
	};

	if (pid == PROC_MAX)
	{
		mutexUnlock(&procTableLock);
		procUnref(child);
		kfree(info);
		return -EAGAIN;
	};

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

	// reset signal dispositions
	schedResetSigActions();
	
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