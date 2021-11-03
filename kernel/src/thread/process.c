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

	// pre-allocate memory for the '1' entry
	if (treemapSet(threads, 1, NULL) != 0)
	{
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
		treemapDestroy(threads);
		kfree(child);
		kfree(info);
		return -ENOMEM;
	};

	memset(newPML4, 0, PAGE_SIZE);

	newPML4[509] = myPML4[509];
	newPML4[510] = myPML4[510];
	newPML4[511] = pagetabGetPhys(myPML4) | PT_PRESENT | PT_WRITE | PT_NOEXEC;

	// TODO: if the calling process is non-kernel, clone the page table

	// fill out the process structure
	memset(child, 0, sizeof(Process));
	child->cr3 = pagetabGetPhys(myPML4);
	child->pagetabVirt = newPML4;
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