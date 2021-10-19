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
void _schedYield(SpinIrqState irqState);

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
		SpinIrqState irqState = spinlockAcquire(&schedLock);
		
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

	// TODO: stuff with the idle thread?

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
	SpinIrqState irqState = spinlockAcquire(&schedLock);

	CPU *cpu = cpuGetCurrent();
	Thread *currentThread = cpu->currentThread;

	currentThread->wakeCounter--;
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
				pagetabGetCR3(cpu->kernelCR3);
			};
			
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
	SpinIrqState irqState = spinlockAcquire(&schedLock);
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

	// make that the retstack
	thread->retstack = frame;

	// wake the thread up
	schedWake(thread);

	// done
	return thread;	
};

noreturn void schedExitThread()
{
	spinlockAcquire(&schedLock);
	
	Thread *currentThread = schedGetCurrentThread();
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

void schedJoinKernelThread(Thread *thread)
{
	SpinIrqState irqState = spinlockAcquire(&schedLock);

	while (thread->retstack != NULL)
	{
		thread->joiner = schedGetCurrentThread();
		spinlockRelease(&schedLock, irqState);
		schedSuspend();

		irqState = spinlockAcquire(&schedLock);
	};

	spinlockRelease(&schedLock, irqState);
	schedDestroyThread(thread);
};

void schedDetachKernelThread(Thread *thread)
{
	SpinIrqState irqState = spinlockAcquire(&schedLock);

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
		
		// ensure the cleanup that is woken up when this exits
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
	SpinIrqState irqState = spinlockAcquire(&schedLock);

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