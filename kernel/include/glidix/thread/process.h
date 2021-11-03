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

/**
 * The kernel init action for initialising the process table and starting `init`.
 */
#define	KIA_PROCESS_INIT					"procInit"

/**
 * Maximum number of processes allowed.
 */
#define	PROC_MAX						(1 << 24)

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
	 * Parent process ID.
	 */
	pid_t parent;

	/**
	 * The process ID.
	 */
	pid_t pid;

	/**
	 * Set of pending signals for this process (will be dispatched to an arbitrary thread).
	 */
	ksigset_t sigPending;

	/**
	 * For each pending signal, the signal information.
	 */
	ksiginfo_t sigInfo[SIG_NUM];

	/**
	 * UIDs and GIDs.
	 */
	uid_t euid, suid, ruid;
	gid_t egid, sgid, rgid;

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

#endif