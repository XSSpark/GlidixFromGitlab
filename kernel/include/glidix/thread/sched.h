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

#ifndef __glidix_hw_sched_h
#define	__glidix_hw_sched_h

#include <glidix/util/common.h>
#include <glidix/util/treemap.h>
#include <glidix/int/signal.h>
#include <glidix/hw/fpu.h>

/**
 * Time quantum in nanoseconds.
 */
#define	SCHED_QUANTUM_NANO			35000000UL

/**
 * Number of scheduler runqueues.
 */
#define	SCHED_NUM_QUEUES			16

/**
 * Default kernel stack size.
 */
#define	SCHED_KERNEL_STACK_SIZE			(2 * 1024 * 1024 - 4096)

/**
 * Entry point to a kernel thread.
 */
typedef void (*KernelThreadFunc)(void *param);

/**
 * Thread return value.
 */
typedef uint64_t thretval_t;

/**
 * Typedef all the structs here.
 */
typedef	struct Thread_ Thread;
typedef struct Process_ Process;			// process.h
typedef struct Runqueue_ Runqueue;

/**
 * Syscall return context. This is the format of the stack frame pushed by `syscall.asm`
 * (see there).
 */
typedef struct
{
	uint64_t ignore;
	uint64_t rbx;
	uint64_t rbp;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t rsp;
	uint64_t rflags;
	uint64_t rip;
	FPURegs fpuRegs;
} SyscallContext;

/**
 * Represents a running thread. Note that some fields required specific offsets as they
 * are access from assembly; these are marked with a comment speicfiyng the offset.
 */
struct Thread_
{
	/**
	 * The syscall return context, needed to dispatch signals from a syscall.
	 */
	SyscallContext *syscallContext;						// 0x00

	/**
	 * Next thread in the runqueue.
	 */
	Thread *next;

	/**
	 * Previous/next thread in the detach list.
	 */
	Thread *detPrev;
	Thread *detNext;

	/**
	 * The wake counter of this thread.
	 */
	int wakeCounter;

	/**
	 * This is set to 1 when the thread is detached.
	 */
	int isDetached;

	/**
	 * The stack pointer to return to. If this is NULL, it means the thread
	 * terminated.
	 */
	void *retstack;

	/**
	 * The kernel stack; to be freed when the thread is joined.
	 */
	void *kernelStack;

	/**
	 * Size of the kernel stack.
	 */
	size_t kernelStackSize;

	/**
	 * The thread trying to join this one (will be woken up when this exits).
	 */
	Thread *joiner;

	/**
	 * The process we are inside of; or `NULL` if this is a kernel thread.
	 */
	Process *proc;

	/**
	 * Set of blocked signals for this thread (SIGKILL and SIGTERM are never set!).
	 */
	ksigset_t sigBlocked;

	/**
	 * Set of currently-pending signals for this thread.
	 */
	ksigset_t sigPending;

	/**
	 * For each pending signal, the signal information.
	 */
	ksiginfo_t sigInfo[SIG_NUM];

	/**
	 * The value of FSBASE for this thread.
	 */
	uint64_t fsbase;

	/**
	 * The thread ID (only applicable to userspace threads).
	 */
	thid_t thid;

	/**
	 * The thread return value. This is set before the thread fully terminates.
	 */
	thretval_t retval;
};

/**
 * Represents a runqueue.
 */
struct Runqueue_
{
	Thread *first;
	Thread *last;
};

/**
 * Stack frame initialising a thread (registers and return address are popped by `_schedReturn()`,
 * to jump to the thread initializer).
 */
typedef struct
{
	IrqState irqState;			// IRQ state
	KernelThreadFunc func;			// r15
	void *param;				// r14
	void *ignored[5];			// r13, r12, rbp, rbx, dummy
	FPURegs fpuRegs;			// FPU regs
	uint64_t dummy;				// the other dummy
	void *entry;				// rip
} ThreadInitialStackFrame;

/**
 * Perform global initialization of the scheduler; do this before
 * initializing the CPU subsystem!
 */
void schedInitGlobal();

/**
 * Perform local (per-CPU) initialization of the scheduler.
 */
void schedInitLocal();

/**
 * Indicate a reason for this thread to suspend was reached. This function might
 * sleep until a new reason to wake up arrives.
 */
void schedSuspend();

/**
 * Indicate to the specified thread a reason to wake up.
 */
void schedWake(Thread *thread);

/**
 * Create a new kernel thread. `func` will be called with `param` in the new thread,
 * and the thread shall exit once that returns. `resv` is reserved and must be NULL.
 * 
 * Returns a thread handle on success, or `NULL` if there was not enough memory to
 * allocate threading structures.
 * 
 * You must renounce ownership of the thread by calling either `schedJoinKernelThread()`,
 * or `schedDetachKernelThread()`.
 */
Thread* schedCreateKernelThread(KernelThreadFunc func, void *param, void *resv);

/**
 * Exit the calling thread, passing the specified value to the joiner (if applicable).
 */
noreturn void schedExitThread(thretval_t retval);

/**
 * Get the calling thread.
 */
Thread* schedGetCurrentThread();

/**
 * Wait for the specified kernel thread to terminate. This function takes ownership of the
 * thread structure, and so you must not use it again after this call. Returns the value that
 * the thread passed to `schedExitThread()`; if the thread exited by returning from its thread
 * function, that value will be 0.
 */
thretval_t schedJoinKernelThread(Thread *thread);

/**
 * Detach from the specified thread. This indicates that we will not be joining this thread,
 * and we don't have to worry about cleaning up its resources once it exits; a separate cleanup
 * thread will manage that.
 */
void schedDetachKernelThread(Thread *thread);

/**
 * Initialize the scheduling timer.
 */
void schedInitTimer();

/**
 * Pre-empt the current thread; this is called from the APIC timer IRQ handler, and so is
 * async-interrupt-safe.
 */
void schedPreempt();

/**
 * Returns nonzero if there are signals ready to dispatch for the current thread/process
 * (i.e. pending and not blocked).
 */
int schedHaveReadySigs();

/**
 * Get the effective user ID of the current process. Kernel is always root.
 */
uid_t schedGetEffectiveUID();

/**
 * Get the effective group ID of the current process. Kernel is always root.
 */
gid_t schedGetEffectiveGID();

/**
 * Set FSBASE for the calling thread.
 */
void schedSetFSBase(uint64_t fsbase);

/**
 * If `act` is not NULL, set the signal dispostion of `signum` to it. If `oldact` is not NULL, the old disposition
 * is stored there. Returns 0 on success, or a negated error number on error.
 */
int schedSigAction(int signum, const SigAction *act, SigAction *oldact);

/**
 * Reset signal dispositions.
 */
void schedResetSigActions();

/**
 * Get the signal mask of the calling thread.
 */
ksigset_t schedGetSigMask();

/**
 * Dispatch a signal to the calling thread, with the specified userspace regs. This will exit from kernel space
 * if necessary and call the signal handler. It will return if the signal is ignored.
 */
void schedDispatchSignal(kmcontext_gpr_t *gprs, FPURegs *fpuRegs, ksiginfo_t *siginfo);

/**
 * If there are any unblocked, pending signals for the current thread/process, remove an arbitrary signal from
 * the pending set, fill in `si` with it, and return 0. Otherwise, return -1.
 */
int schedCheckSignals(ksiginfo_t *si);

/**
 * Deliver a signal to a process.
 */
void schedDeliverSignalToProc(Process *proc, ksiginfo_t *si);

/**
 * Deliver a signal to a thread.
 */
void schedDeliverSignalToThread(Thread *thread, ksiginfo_t *si);

#endif