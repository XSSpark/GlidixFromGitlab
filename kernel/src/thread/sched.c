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

#include <glidix/thread/sched.h>
#include <glidix/util/memory.h>
#include <glidix/hw/cpu.h>
#include <glidix/util/panic.h>
#include <glidix/util/string.h>
#include <glidix/thread/spinlock.h>
#include <glidix/util/time.h>
#include <glidix/hw/apic.h>
#include <glidix/hw/idt.h>
#include <glidix/hw/pagetab.h>
#include <glidix/hw/msr.h>
#include <glidix/thread/process.h>

/**
 * The userspace aux code for returning from a signal handler.
 */
extern char userAuxSigReturn[];

/**
 * The global scheduler lock.
 */
static Spinlock schedLock;

/**
 * Maps a CPU index to whether or not the CPU is idling.
 */
static int schedIdling[CPU_MAX];

/**
 * Runqueues based on priority.
 */
static Runqueue runqueues[SCHED_NUM_QUEUES];

/**
 * The cleanup thread.
 */
static Thread* schedGlobalCleanupThread;

/**
 * The head of the 'detached thread list', where threads are added when they
 * are detached, so the cleanup thread can scan them.
 */
static Thread* schedDetHead;

/**
 * A quantum of time.
 */
static uint32_t schedQuantum;

/**
 * In sched.asm: The code to jump to when entering a new thread.
 */
extern char _schedThreadEntry[];

/**
 * In sched.asm: get a 'save stack' and call `_schedNext()`.
 */
void _schedYield(IrqState irqState);

/**
 * In sched.asm: take a 'save stack' found by `_schedYield()`, and return
 * to the context.
 */
noreturn void _schedReturn(void *stack);

/**
 * In sched.asm: take a stack pointer and go into the 'idle' state.
 */
noreturn void _schedIdle(void *stack);

/**
 * In asched.asm: enter a userspace signal handler.
 */
noreturn void _schedEnterSignalHandler(int signum, user_addr_t siginfoAddr, user_addr_t contextAddr, user_addr_t rip);

/**
 * In sched.asm: update the TSS, for the specified kernel stack.
 */
void _schedUpdateTSS(void *kernelStack);

/**
 * Destroy a terminated thread (call this only from the context of another
 * thread, without the schedLock!).
 */
static void schedDestroyThread(Thread *thread)
{
	kfree(thread->kernelStack);
	kfree(thread);
};

/**
 * The cleanup loop, running in a special thread to clean up detached
 * threads.
 */
static void schedCleanup()
{
	while (1)
	{
		IrqState irqState = spinlockAcquire(&schedLock);
		
		Thread *thread;
		for (thread=schedDetHead; thread!=NULL; thread=thread->next)
		{
			if (thread->retstack == NULL)
			{
				// this one exited
				if (thread->detPrev == NULL)
				{
					schedDetHead = thread->detNext;
				}
				else
				{
					thread->detPrev->detNext = thread->detNext;
				};

				if (thread->detNext != NULL)
				{
					thread->detNext->detPrev = thread->detPrev;
				};

				break;
			};
		};

		spinlockRelease(&schedLock, irqState);
		
		if (thread != NULL)
		{
			schedDestroyThread(thread);
		};

		schedSuspend();
	};
};

void schedInitGlobal()
{
	schedGlobalCleanupThread = schedCreateKernelThread(schedCleanup, NULL, NULL);
};

void schedInitLocal()
{
	CPU *cpu = cpuGetCurrent();

	Thread *initThread = (Thread*) kmalloc(sizeof(Thread));
	if (initThread == NULL)
	{
		panic("ran out of memory while initializing scheduling locally!\n");
	};

	memset(initThread, 0, sizeof(Thread));
	initThread->wakeCounter = 1;
	initThread->kernelStack = cpu->startupStack;
	initThread->kernelStackSize = CPU_STARTUP_STACK_SIZE;

	// make us the current thread
	cpu->currentThread = initThread;

	// activate the APIC timer if necessary
	if (schedQuantum != 0)
	{
		apic.lvtTimer = I_APIC_TIMER;
		__sync_synchronize();
		apic.timerInitCount = schedQuantum;
		__sync_synchronize();
	};
};

void schedSuspend()
{
	IrqState irqState = spinlockAcquire(&schedLock);

	CPU *cpu = cpuGetCurrent();
	Thread *currentThread = cpu->currentThread;

	currentThread->wakeCounter--;
	if (currentThread->wakeCounter < 0)
	{
		// this can happen in the idle thread as it's special
		currentThread->wakeCounter = 0;
	};
	
	if (currentThread->wakeCounter == 0)
	{
		// yield to the next task; note that when this returns, we will no longer
		// have the schedLock, so don't try to double-release it!
		_schedYield(irqState);
	}
	else
	{
		// we are not yielding yet, so release the spinlock and keep going
		spinlockRelease(&schedLock, irqState);
	};
};

/**
 * Called by sched.asm: we are holding the schedLock with interrupts disabled, and we must
 * find the next thread to schedule, and call `_schedReturn()` with it.
 */
noreturn void _schedNext(void *stack)
{
	CPU *cpu = cpuGetCurrent();
	cpu->currentThread->retstack = stack;

	int myCpuIndex = cpuGetMyIndex();
	schedIdling[myCpuIndex] = 0;

	int i;
	for (i=0; i<SCHED_NUM_QUEUES; i++)
	{
		Runqueue *q = &runqueues[i];
		if (q->first != NULL)
		{
			Thread *nextThread = q->first;
			q->first = nextThread->next;
			if (q->first == NULL) q->last = NULL;

			cpu->currentThread = nextThread;

			// switch to the correct CR3
			if (nextThread->proc != NULL)
			{
				pagetabSetCR3(nextThread->proc->cr3);
			}
			else
			{
				pagetabSetCR3(cpu->kernelCR3);
			};
			
			// set the FSBASE
			wrmsr(MSR_FS_BASE, nextThread->fsbase);

			// update the TSS and the syscall stack
			void *kernelRSP = (char*) nextThread->kernelStack + nextThread->kernelStackSize;
			_schedUpdateTSS(kernelRSP);
			cpu->syscallStackPointer = kernelRSP;

			// release the schedLock, but keep interrupts disabled
			spinlockRelease(&schedLock, 0);

			// reset the timer
			if (schedQuantum != 0) apic.timerInitCount = schedQuantum;

			// return into the thread
			_schedReturn(nextThread->retstack);
		};
	};

	// release the spinlock, but keep interrupts disabled, then go into the
	// idle state (which will enable interrupts)
	cpu->currentThread = &cpu->idleThread;
	cpu->idleThread.wakeCounter = 1;
	schedIdling[myCpuIndex] = 1;
	pagetabSetCR3(cpu->kernelCR3);
	spinlockRelease(&schedLock, 0);
	_schedIdle(cpu->idleStack + CPU_IDLE_STACK_SIZE);
};

/**
 * Wake up the specified thread; please call this only while holding schedLock!
 */
static void _schedWake(Thread *thread)
{
	if (thread->wakeCounter++ != 0)
	{
		// thread is already awake
		return;
	};

	// we will be at the end of a runqueue
	thread->next = NULL;

	// TODO: determine priority and therefore which queue to use
	Runqueue *q = &runqueues[0];
	if (q->last == NULL)
	{
		q->first = q->last = thread;
	}
	else
	{
		q->last->next = thread;
		q->last = thread;
	};

	// if any CPUs are idling, wake one
	int myCpuIndex = cpuGetMyIndex();
	int i;
	for (i=0; i<cpuGetCount(); i++)
	{
		if (i != myCpuIndex)
		{
			if (schedIdling[i])
			{
				schedIdling[i] = 0;
				cpuWake(i);
				break;
			};
		};
	};
};

void schedWake(Thread *thread)
{
	IrqState irqState = spinlockAcquire(&schedLock);
	_schedWake(thread);
	spinlockRelease(&schedLock, irqState);
};

Thread* schedCreateKernelThread(KernelThreadFunc func, void *param, void *resv)
{
	size_t stackSize = SCHED_KERNEL_STACK_SIZE;

	void *kernelStack = kmalloc(stackSize);
	if (kernelStack == NULL)
	{
		return NULL;
	};

	Thread *thread = (Thread*) kmalloc(sizeof(Thread));
	if (thread == NULL)
	{
		kfree(kernelStack);
		return NULL;
	};

	memset(thread, 0, sizeof(Thread));

	thread->kernelStack = kernelStack;
	thread->kernelStackSize = stackSize;

	// create the initial stack frame
	uint64_t rsp = (uint64_t) kernelStack + stackSize;
	rsp &= ~0xFUL;
	rsp -= sizeof(ThreadInitialStackFrame);

	ThreadInitialStackFrame *frame = (ThreadInitialStackFrame*) rsp;
	frame->irqState = (1 << 9);	// enable interrupts in the new thread
	frame->func = func;
	frame->param = param;
	frame->entry = _schedThreadEntry;
	fpuSave(&frame->fpuRegs);

	// make that the retstack
	thread->retstack = frame;

	// wake the thread up
	schedWake(thread);

	// done
	return thread;	
};

noreturn void schedExitThread(thretval_t retval)
{
	spinlockAcquire(&schedLock);
	
	Thread *currentThread = schedGetCurrentThread();
	currentThread->retval = retval;
	if (currentThread->joiner != NULL)
	{
		_schedWake(currentThread->joiner);
	};

	_schedNext(NULL);
};

/**
 * This function CAN be called when holding schedLock!
 */
Thread* schedGetCurrentThread()
{
	return cpuGetCurrent()->currentThread;
};

thretval_t schedJoinKernelThread(Thread *thread)
{
	IrqState irqState = spinlockAcquire(&schedLock);

	while (thread->retstack != NULL)
	{
		thread->joiner = schedGetCurrentThread();
		spinlockRelease(&schedLock, irqState);
		schedSuspend();

		irqState = spinlockAcquire(&schedLock);
	};

	spinlockRelease(&schedLock, irqState);

	thretval_t retval = thread->retval;
	schedDestroyThread(thread);
	return retval;
};

void schedDetachKernelThread(Thread *thread)
{
	IrqState irqState = spinlockAcquire(&schedLock);

	thread->isDetached = 1;
	if (thread->retstack == NULL)
	{
		// already exited
		spinlockRelease(&schedLock, irqState);
		schedDestroyThread(thread);
	}
	else
	{
		// add to the detached list
		thread->detNext = schedDetHead;
		thread->detPrev = NULL;
		if (schedDetHead != NULL) schedDetHead->detPrev = thread;
		schedDetHead = thread;
		
		// ensure the cleanup thread is woken up when this exits
		thread->joiner = schedGlobalCleanupThread;
		spinlockRelease(&schedLock, irqState);
	};
};

#include <glidix/util/log.h>
void schedInitTimer()
{
	apic.timerDivide = 3;
	__sync_synchronize();
	apic.timerInitCount = 0xFFFFFFFF;
	__sync_synchronize();
	
	nanoseconds_t start = timeGetUptime();
	while (timeGetUptime() < start+SCHED_QUANTUM_NANO);

	apic.lvtTimer = 0;
	__sync_synchronize();
	schedQuantum = 0xFFFFFFFF - apic.timerCurrentCount;
	__sync_synchronize();
	apic.timerInitCount = 0;
	__sync_synchronize();

	// put the timer in single-shot mode at the appropriate interrupt vector.
	apic.lvtTimer = I_APIC_TIMER;
	__sync_synchronize();

	// now perform the initial activation of the timer
	apic.timerInitCount = schedQuantum;
	__sync_synchronize();
};

void schedPreempt()
{
	IrqState irqState = spinlockAcquire(&schedLock);

	CPU *cpu = cpuGetCurrent();
	if (cpu->currentThread != &cpu->idleThread)
	{
		// if we are not the idle thread, add us to the end of
		// the runqueue
		// TODO: chooose runqueue based on priority etc
		Runqueue *q = &runqueues[0];
		cpu->currentThread->next = NULL;

		if (q->first == NULL)
		{
			q->first = q->last = cpu->currentThread;
		}
		else
		{
			q->last->next = cpu->currentThread;
			q->last = cpu->currentThread;
		};
	};

	_schedYield(irqState);
};

int schedHaveReadySigs()
{
	Thread *me = schedGetCurrentThread();
	IrqState irqState = spinlockAcquire(&schedLock);

	ksigset_t pending = me->sigPending;
	if (me->proc != NULL) pending |= me->proc->sigPending;

	ksigset_t ready = pending & ~me->sigBlocked;
	spinlockRelease(&schedLock, irqState);

	return !!ready;
};

uid_t schedGetEffectiveUID()
{
	Thread *me = schedGetCurrentThread();
	if (me->proc == NULL) return 0;
	else return me->proc->euid;
};

gid_t schedGetEffectiveGID()
{
	Thread *me = schedGetCurrentThread();
	if (me->proc == NULL) return 0;
	else return me->proc->egid;
};

void schedSetFSBase(uint64_t fsbase)
{
	schedGetCurrentThread()->fsbase = fsbase;
	wrmsr(MSR_FS_BASE, fsbase);
};

int schedSigAction(int signum, const SigAction *act, SigAction *oldact)
{
	if (signum < 1 || signum >= SIG_NUM)
	{
		return -EINVAL;
	};

	IrqState irqState = spinlockAcquire(&schedLock);

	Process *proc = schedGetCurrentThread()->proc;
	if (oldact != NULL)
	{
		memcpy(oldact, &proc->sigActions[signum], sizeof(SigAction));
	};

	if (act != NULL && signum != SIGKILL && signum != SIGSTOP && signum != SIGTHKILL)
	{
		memcpy(&proc->sigActions[signum], act, sizeof(SigAction));
	};

	spinlockRelease(&schedLock, irqState);
	return 0;
};

void schedResetSigActions()
{
	IrqState irqState = spinlockAcquire(&schedLock);
	Process *proc = schedGetCurrentThread()->proc;
	memset(proc->sigActions, 0, sizeof(proc->sigActions));
	spinlockRelease(&schedLock, irqState);
};

user_addr_t schedGetDefaultSignalAction(int signum)
{
	switch (signum)
	{
	case SIGHUP:
	case SIGINT:
	case SIGKILL:
	case SIGPIPE:
	case SIGALRM:
	case SIGTERM:
	case SIGUSR1:
	case SIGUSR2:
	case SIGPOLL:
		return SIG_TERM;
	case SIGQUIT:
	case SIGILL:
	case SIGTRAP:
	case SIGABRT:
	case SIGFPE:
	case SIGBUS:
	case SIGSEGV:
	case SIGSYS:
		return SIG_CORE;
	case SIGSTOP:
	case SIGTSTP:
	case SIGTTIN:
	case SIGTTOU:
		return SIG_STOP;
	default:
		return SIG_IGN;
	};
};

void schedDispatchSignal(kmcontext_gpr_t *gprs, FPURegs *fpuRegs, ksiginfo_t *siginfo)
{
	// for SIGTHKILL, special treatment: exit from the current userspace thread
	if (siginfo->si_signo == SIGTHKILL)
	{
		procExitThread(0);
	};

	// get the signal disposition
	SigAction act;
	int status = schedSigAction(siginfo->si_signo, NULL, &act);
	ASSERT(status == 0);

	user_addr_t handler = act.sa_sigaction_handler;
	if (handler == SIG_DFL) handler = schedGetDefaultSignalAction(siginfo->si_signo);

	if (handler == SIG_IGN)
	{
		// ignore
		return;
	}
	else if (handler == SIG_TERM || handler == SIG_CORE)
	{
		procExit(PROC_WS_SIG(siginfo->si_signo));
	}
	else if (handler == SIG_STOP)
	{
		panic("TODO: implement stopping");
	}
	else if (handler >= 256)
	{
		// user handler; first push the GPRs right above the red area
		user_addr_t gprAddr = gprs->rsp - sizeof(kmcontext_gpr_t) - 128;
		if (procToUserCopy(gprAddr, gprs, sizeof(kmcontext_gpr_t)) != 0)
		{
			procExit(PROC_WS_SIG(SIGKILL));
		};

		// put the signal info on the stack
		user_addr_t siginfoAddr = (gprAddr - sizeof(ksiginfo_t)) & ~0x7;
		if (procToUserCopy(siginfoAddr, siginfo, sizeof(ksiginfo_t)) != 0)
		{
			procExit(PROC_WS_SIG(SIGKILL));
		};

		// now create the `ucontext_t`, ensuring it is 16-bytes-aligned (required for the
		// fpuRegs to work)
		user_addr_t contextAddr = (siginfoAddr - sizeof(kucontext_t)) & ~0xF;
		kucontext_t ucontext;
		memset(&ucontext, 0, sizeof(kucontext_t));

		ucontext.uc_sigmask = schedGetCurrentThread()->sigBlocked;
		memcpy(&ucontext.fpuRegs, fpuRegs, 512);
		ucontext.gprptr = gprAddr;

		if (procToUserCopy(contextAddr, &ucontext, sizeof(kucontext_t)) != 0)
		{
			procExit(PROC_WS_SIG(SIGKILL));
		};

		// mask the signals specified by the action
		schedGetCurrentThread()->sigBlocked |= act.sa_mask;

		// push the return address onto the stack (will point to userAuxSigReturn)
		user_addr_t rsp = contextAddr - 8;
		uint64_t retAddr = (uint64_t) (userAuxSigReturn);
		if (procToUserCopy(rsp, &retAddr, 8) != 0)
		{
			procExit(PROC_WS_SIG(SIGKILL));
		};

		// enter the handler
		_schedEnterSignalHandler(siginfo->si_signo, siginfoAddr, contextAddr, handler);
	};
};

int schedCheckSignals(ksiginfo_t *si)
{
	Thread *me = schedGetCurrentThread();
	IrqState irqState = spinlockAcquire(&schedLock);

	ksigset_t pending = me->sigPending;
	if (me->proc != NULL) pending |= me->proc->sigPending;

	ksigset_t ready = pending & ~me->sigBlocked;
	int i;
	for (i=1; i<SIG_NUM; i++)
	{
		if (ready & (1UL << i))
		{
			if (me->proc != NULL)
			{
				if (me->proc->sigPending & (1UL << i))
				{
					memcpy(si, &me->proc->sigInfo[i], sizeof(ksiginfo_t));
					me->proc->sigPending &= ~(1UL << i);
					spinlockRelease(&schedLock, irqState);
					return 0;
				};
			};

			if (me->sigPending & (1UL << i))
			{
				memcpy(si, &me->sigInfo[i], sizeof(ksiginfo_t));
				me->sigPending &= ~(1UL << i);
				spinlockRelease(&schedLock, irqState);
				return 0;
			};
		};
	};

	spinlockRelease(&schedLock, irqState);
	return -1;
};

void schedDeliverSignalToProc(Process *proc, ksiginfo_t *si)
{
	if (si->si_signo == SIGTHKILL)
	{
		// ignore SIGTHKILL sent to a process
		return;
	};

	IrqState irqState = spinlockAcquire(&schedLock);

	ksigset_t mask = (1UL << si->si_signo);
	SigAction *act = &proc->sigActions[si->si_signo];
	user_addr_t handler = act->sa_sigaction_handler;
	if (handler < 256 && proc->pid == 1)
	{
		// don't deliver signals to init which it doesn't handle
		spinlockRelease(&schedLock, irqState);
		return;
	};
	
	if (handler == SIG_DFL) handler = schedGetDefaultSignalAction(si->si_signo);
	if (handler == SIG_IGN)
	{
		// the signal is ignored, so there's no need to deliver it
		spinlockRelease(&schedLock, irqState);
		return;
	};

	// not ignored, so deliver unless already there
	int signalled = 0;
	if ((proc->sigPending & mask) == 0)
	{
		memcpy(&proc->sigInfo[si->si_signo], si, sizeof(ksiginfo_t));
		proc->sigPending |= mask;
		signalled = 1;
	};

	spinlockRelease(&schedLock, irqState);
	if (signalled) cpuInformProcSignalled(proc);
};

void schedDeliverSignalToThread(Thread *thread, ksiginfo_t *si)
{
	IrqState irqState = spinlockAcquire(&schedLock);

	ksigset_t mask = (1UL << si->si_signo);
	SigAction *act = &thread->proc->sigActions[si->si_signo];
	user_addr_t handler = act->sa_sigaction_handler;
	if (handler < 256 && thread->proc->pid == 1 && si->si_signo != SIGTHKILL)
	{
		// don't deliver signals to init which it doesn't handle
		spinlockRelease(&schedLock, irqState);
		return;
	};
	
	if (handler == SIG_DFL) handler = schedGetDefaultSignalAction(si->si_signo);
	if (handler == SIG_IGN)
	{
		// the signal is ignored, so there's no need to deliver it
		spinlockRelease(&schedLock, irqState);
		return;
	};

	// not ignored, so deliver unless already there
	int signalled = 0;
	if ((thread->sigPending & mask) == 0)
	{
		memcpy(&thread->sigInfo[si->si_signo], si, sizeof(ksiginfo_t));
		thread->sigPending |= mask;
		signalled = 1;
	};

	spinlockRelease(&schedLock, irqState);
	if (signalled) cpuInformThreadSignalled(thread);
};