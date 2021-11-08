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

#ifndef __glidix_thread_process_h
#define	__glidix_thread_process_h

#include <glidix/thread/sched.h>
#include <glidix/fs/vfs.h>
#include <glidix/fs/path.h>
#include <glidix/fs/file.h>
#include <glidix/int/signal.h>

/**
 * The kernel init action for initialising the process table and starting `init`.
 */
#define	KIA_PROCESS_INIT					"procInit"

/**
 * Maximum number of processes allowed.
 */
#define	PROC_MAX						(1 << 24)

/**
 * Maximum address for userspace mappings.
 * 
 * This is set to be 44 bits long, so that upon discarding the bottom 12 bits, we get a
 * 32-bit 'page index', which we are using with a treemap. The implementation of the mapping
 * tree must change if we want to make this longer.
 */
#define	PROC_USER_ADDR_MAX					(1UL << 44)

/**
 * Protection settings.
 */
#ifndef PROT_READ
#define	PROT_READ						(1 << 0)
#define	PROT_WRITE						(1 << 1)
#define	PROT_EXEC						(1 << 2)
#define	PROT_ALL						((1 << 3)-1)
#endif

/**
 * Only define those if they weren't yet defined, since we might have been included by
 * a userspace application with <sys/mman.h> already included.
 */
#ifndef MAP_FAILED
#define	MAP_PRIVATE						(1 << 0)
#define	MAP_SHARED						(1 << 1)
#define	MAP_ANON						(1 << 2)
#define	MAP_FIXED						(1 << 3)
#define	MAP_ALLFLAGS						((1 << 4)-1)
#define	MAP_FAILED						((uint64_t)-1)
#endif

/**
 * Type representing a userspace address. Never cast these to pointers, as userspace addresses
 * are NOT to be trusted!
 */
typedef uint64_t user_addr_t;

/**
 * Process startup information.
 */
typedef struct
{
	/**
	 * The FSBASE to use for the initial thread.
	 */
	uint64_t fsbase;

	/**
	 * The function to call.
	 */
	KernelThreadFunc func;

	/**
	 * The parameter to pass to the function.
	 */
	void *param;

	/**
	 * The process.
	 */
	Process *proc;
} ProcessStartupInfo;

/**
 * Represents a process memory mapping. These objects are immutable, except for the `refcount`
 * field, and so can be reused when forking etc.
 */
typedef struct
{
	/**
	 * The refcount. This basically indicates how many pages across any number of processes
	 * are using this exact mapping.
	 */
	int refcount;

	/**
	 * The file open flags (`O_*`, this is used to control when we can set prots etc).
	 */
	int oflags;

	/**
	 * The inode. We are holding a reference to it, and this will be unreffed once this mapping
	 * is unmapped from all pages. This is NULL in the special "anonymous mapping".
	 */
	Inode *inode;

	/**
	 * The user base address where this mapping begins (i.e. corresponds to `offset`).
	 */
	user_addr_t addr;

	/**
	 * The offset within the inode (corresponding to `addr`).
	 */
	off_t offset;

	/**
	 * Mapping flags (`MAP_*`).
	 */
	int mflags;
} ProcessMapping;

/**
 * Represents a process (a collection of userspace threads sharing a single address space).
 */
struct Process_
{
	/**
	 * Physical address of the page table.
	 */
	uint64_t cr3;

	/**
	 * Pointer to the page table KOM object.
	 */
	void *pagetabVirt;

	/**
	 * Tree map, mapping "page indices" to a `ProcessMapping*`.
	 */
	TreeMap *mappingTree;

	/**
	 * Mutex protecting the address space.
	 */
	Mutex mapLock;

	/**
	 * Parent process ID.
	 */
	pid_t parent;

	/**
	 * The process ID.
	 */
	pid_t pid;

	/**
	 * Set of pending signals for this process (will be dispatched to an arbitrary thread).
	 * 
	 * This is protected by the scheduler lock.
	 */
	ksigset_t sigPending;

	/**
	 * For each pending signal, the signal information.
	 * 
	 * This is protected by the scheduler lock.
	 */
	ksiginfo_t sigInfo[SIG_NUM];

	/**
	 * Signal dispositions for the current process.
	 * 
	 * This is protected by the scheduler lock.
	 */
	SigAction sigActions[SIG_NUM];

	/**
	 * UIDs and GIDs.
	 */
	uid_t euid, suid, ruid;
	gid_t egid, sgid, rgid;

	/**
	 * Lock protecting the root and current dirs.
	 */
	Mutex dirLock;

	/**
	 * Path walker pointing to the root directory.
	 */
	PathWalker rootDir;

	/**
	 * Path walker pointing to the current working directory.
	 */
	PathWalker currentDir;

	/**
	 * The thread table (of threads running in the process).
	 */
	TreeMap *threads;

	/**
	 * Lock for the thread table.
	 */
	Mutex threadTableLock;

	/**
	 * Reference count.
	 */
	int refcount;

	/**
	 * Number of threads running.
	 */
	int numThreads;
};

/**
 * Context of page cloning.
 */
typedef struct
{
	/**
	 * The parent process (the current process).
	 */
	Process *parent;

	/**
	 * The mapping tree of the child.
	 */
	TreeMap *childTree;

	/**
	 * The child PML4.
	 */
	void *childPageTable;

	/**
	 * Initially set to 0, set to an error number if one occurs.
	 */
	errno_t err;
} PageCloneContext;

/**
 * Create a new process.
 * 
 * The new process inherits the majority of the calling process' information, such as root dir,
 * working dir, etc., and it gets a copy of all current mappings, with private mappings being
 * copy-on-write, such that each process sees its own copy of the user part of the address space.
 * 
 * This is used to implement `fork()`. From the kernel perspective, this will essentially create
 * a new thread, which will be part of a new process, and `func(param)` is called inside the new
 * thread.
 * 
 * Returns the (positive) pid of the new process on success, or a negated error number on error.
 */
pid_t procCreate(KernelThreadFunc func, void *param);

/**
 * Decrement the refcount of a process object.
 */
void procUnref(Process *proc);

/**
 * Create a file mapping or an anonymous mapping in the address space of the calling process.
 * 
 * `addr` must be page-aligned, and `addr+length` must not exceed `PROC_USER_ADDR_MAX`. The mapping
 * will always be made at the requested address, with one exception depending on whether `MAP_FIXED`
 * is set in `flags`. If `MAP_FIXED` is set, `addr` will always be the address used, and you can even
 * map NULL. If `MAP_FIXED` is not set, then if `addr` is zero, the kernel will automatically allocate
 * enough address space to map `length` without overlapping existing segments.
 * 
 * If `MAP_ANON` is set in `flags`, `fp` must be NULL and `offset` is ignored. In this case, an anonymous
 * mapping is created, and accessing the mapped memory range will initially read zeroes. If `MAP_ANON` is
 * not set in `flags`, then `fp` must not be NULL, and `offset` must be a page-aligned offset in the file.
 * Accessing the mapped range in this case initally reads the contents of the file at the corresponding offset.
 * 
 * Either `MAP_PRIVATE` or `MAP_SHARED` must be set in `flags`. If `MAP_PRIVATE` is set, then the mapping
 * is private; any modification to the contents of the pages is visible only to the current process, and is
 * not committed to disk. On the other hand, with a `MAP_SHARED` mapping, changes will be visible to all
 * processes which map the same file as shared, and the changes will be committed to disk.
 * 
 * If the function fails and returns an error, some of the requested address space might be mapped.
 * 
 * On success, this function will return the user address where the new mapping begins (which might be zero).
 * On error, `MAP_FAILED` is returned, and if `err` is not NULL, the error number is stored there.
 */
user_addr_t procMap(user_addr_t addr, size_t length, int prot, int flags, File *fp, off_t offset, errno_t *err);

/**
 * Perform pre-exec cleanup.
 * 
 * Unmaps all userspace segments, resets signal dispositions, closes close-on-exec files, etc.
 */
void procBeginExec();

/**
 * Handle a page fault for the specified address. `addr` is the address which faulted. Returns 0 if the page
 * fault has been resolved and the program should be allowed to continue, or -1 on error. If `siginfo` is not
 * NULL, and an error occurs, it is filled in with details of the signal to be dispatched.
 */
int procPageFault(user_addr_t addr, int faultFlags, ksiginfo_t *siginfo);

/**
 * Copy into kernel memory, from the userspace address `addr`. Returns 0 on success, or a negated error number
 * (probably `-EFAULT`) if the copy was not possible.
 */
int procToKernelCopy(void *ptr, user_addr_t addr, size_t size);

/**
 * Copy into user memory, from the kernel pointer. Returns 0 on success, or a negated error number (probably
 * `-EFAULT`) if the copy was not possible.
 */
int procToUserCopy(user_addr_t addr, const void *ptr, size_t size);

/**
 * Get (and upref) a process given a pid. Returns NULL if no such process exists. Remember to call `procUnref()`
 * on the returned handle later.
 */
Process* procByPID(pid_t pid);

/**
 * Increment the reference count of the process, and return it again.
 */
Process* procDup(Process *proc);

#endif