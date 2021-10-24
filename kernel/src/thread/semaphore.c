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

#include <glidix/thread/semaphore.h>
#include <glidix/util/errno.h>
#include <glidix/util/panic.h>

void semInit(Semaphore *sem)
{
	semInit2(sem, 1);
};

void semInit2(Semaphore *sem, int count)
{
	spinlockInit(&sem->lock);
	sem->count = count;
	sem->flags = 0;
	sem->first = NULL;
	sem->last = NULL;
};

static void _semQueue(Semaphore *sem, SemWaiter *waiter)
{
	waiter->prev = waiter->next = NULL;

	if (sem->first == NULL)
	{
		sem->first = sem->last = waiter;
	}
	else
	{
		waiter->prev = sem->last;
		sem->last->next = waiter;
		sem->last = waiter;
	};
};

static void _semUnqueue(Semaphore *sem, SemWaiter *waiter)
{
	if (sem->first == waiter) sem->first = waiter->next;
	if (waiter->prev != NULL) waiter->prev->next = waiter->next;
	if (waiter->next != NULL) waiter->next->prev = waiter->prev;
	if (sem->first == NULL) sem->last = NULL;
};

static int _semIsInterrupted(int flags)
{
	if ((flags & SEM_W_INTR) == 0)
	{
		// not interruptible
		return 0;
	};

	return schedHaveReadySigs();
};

int semWaitGen(Semaphore *sem, int count, int flags, nanoseconds_t nanotimeout)
{
	// get the current thread
	Thread *me = schedGetCurrentThread();

	// first handle the case where count is zero
	if (count == 0)
	{
		return -EAGAIN;
	};

	// figure out the deadline (0 meaning there is no deadline)
	nanoseconds_t deadline = nanotimeout == 0 ? 0 : timeGetUptime() + nanotimeout;

	IrqState irqState = spinlockAcquire(&sem->lock);
	
	// if terminated and there are no more resources available, return 0
	if (sem->count == 0 && sem->flags & SEM_TERMINATED)
	{
		spinlockRelease(&sem->lock, irqState);
		return 0;
	};

	// check if we are waiting on all resources currently available
	if (count == -1)
	{
		if (sem->count == 0)
		{
			// none available, return the EAGAIN error
			spinlockRelease(&sem->lock, irqState);
			return -EAGAIN;
		}
		else
		{
			// some are available, wait on them all
			count = sem->count;
		};
	};

	// if there are available resources, return them
	if (sem->count != 0)
	{
		int taking = count;
		if (sem->count < taking) taking = sem->count;

		sem->count -= taking;
		spinlockRelease(&sem->lock, irqState);

		return taking;
	};

	// the semaphore is not terminated, and there are no available resources,
	// and we can block, so add us to the queue
	SemWaiter waiter;
	waiter.thread = me;
	waiter.requested = count;
	waiter.given = 0;
	waiter.signalled = 0;
	_semQueue(sem, &waiter);

	// set the timer
	TimedEvent ev;
	timedPost(&ev, deadline);

	// keep going to sleep until we either received the resources, or were
	// interrupted
	while (!waiter.signalled && !_semIsInterrupted(flags) && (timeGetUptime() < deadline || deadline == 0))
	{
		spinlockRelease(&sem->lock, irqState);
		schedSuspend();
		irqState = spinlockAcquire(&sem->lock);
	};

	// remove us from the queue if we were not signalled
	if (!waiter.signalled)
	{
		_semUnqueue(sem, &waiter);
	};

	// we can now release the spinlock
	spinlockRelease(&sem->lock, irqState);

	// cancel the timer
	timedCancel(&ev);

	if (waiter.signalled)
	{
		// if the waiter WAS signalled, then `given` is the number of units
		// we were given (and must thus return); and it'll be 0 if terminated!
		return waiter.given;
	}
	else
	{
		// figure out which reason caused us to quit
		if (_semIsInterrupted(flags))
		{
			return -EINTR;
		}
		else
		{
			// we timed out
			return -ETIMEDOUT;
		};
	};
};

void semWait(Semaphore *sem)
{
	if (semWaitGen(sem, 1, 0, 0) != 1)
	{
		panic("semWait() called and the semaphore was terminated!");
	};
};

void semSignal(Semaphore *sem)
{
	semSignal2(sem, 1);
};

void semSignal2(Semaphore *sem, int count)
{
	IrqState irqState = spinlockAcquire(&sem->lock);

	while (sem->first != NULL && count > 0)
	{
		int giving = count;
		if (sem->first->requested < giving) giving = sem->first->requested;

		sem->first->given = giving;
		sem->first->signalled = 1;
		schedWake(sem->first->thread);

		_semUnqueue(sem, sem->first);
		count -= giving;
	};

	sem->count += count;
	spinlockRelease(&sem->lock, irqState);
};

void semTerminate(Semaphore *sem)
{
	IrqState irqState = spinlockAcquire(&sem->lock);

	sem->flags |= SEM_TERMINATED;
	while (sem->first != NULL)
	{
		// tell them that they received 0 resources
		sem->first->given = 0;
		sem->first->signalled = 1;
		schedWake(sem->first->thread);
		_semUnqueue(sem, sem->first);
	};

	spinlockRelease(&sem->lock, irqState);
};

int semPoll(int numSems, Semaphore **sems, uint8_t *bitmap, int flags, nanoseconds_t nanotimeout)
{
	Thread *me = schedGetCurrentThread();
	if (numSems > SEM_POLL_MAX)
	{
		return -EINVAL;
	};

	// initialize a waiter struct for each semaphore we are polling
	SemWaiter waiters[SEM_POLL_MAX];
	int i;
	for (i=0; i<numSems; i++)
	{
		SemWaiter *waiter = &waiters[i];
		waiter->thread = me;
		waiter->requested = 0;
		waiter->given = 0;
		waiter->signalled = 0;
	};

	// set up the timer
	nanoseconds_t deadline = nanotimeout == 0 ? 0 : timeGetUptime() + nanotimeout;
	TimedEvent ev;
	timedPost(&ev, deadline);

	// enqueue us to each semaphore, or declare it as already free
	for (i=0; i<numSems; i++)
	{
		Semaphore *sem = sems[i];
		if (sem != NULL)
		{
			IrqState irqState = spinlockAcquire(&sem->lock);
			if (sem->flags & SEM_TERMINATED || sem->count != 0)
			{
				bitmap[i/8] |= (1 << (i%8));
				waiters[i].signalled = 1; 		// indicate that we've left
			}
			else
			{
				// enqueue us
				_semQueue(sem, &waiters[i]);
			};
			spinlockRelease(&sem->lock, irqState);
		};
	};

	// wait for semaphores to become free
	while (1)
	{
		// check if any have become signalled
		for (i=0; i<numSems; i++)
		{
			Semaphore *sem = sems[i];
			if (sem != NULL)
			{
				IrqState irqState = spinlockAcquire(&sem->lock);
				int signalled = waiters[i].signalled;
				spinlockRelease(&sem->lock, irqState);
				if (signalled) break;
			};
		};

		// if we found a signalled semaphore, break out of the loop
		if (i != numSems) break;

		// if we were interrupted, break out
		if (_semIsInterrupted(flags)) break;

		// if we timed out, break out
		if (timeGetUptime() >= deadline && deadline != 0) break;

		// otherwise we suspend
		schedSuspend();
	};

	// cancel the timer
	timedCancel(&ev);

	// count how many semaphores are free, and unqueue us where necessary
	int numFreeSems = 0;
	for (i=0; i<numSems; i++)
	{
		Semaphore *sem = sems[i];
		if (sem != NULL)
		{
			IrqState irqState = spinlockAcquire(&sem->lock);
			if (!waiters[i].signalled)
			{
				// not signalled, so must be unqueued
				_semUnqueue(sem, &waiters[i]);
			}
			else
			{
				// add us to the number of free semaphores
				numFreeSems++;
			};
			spinlockRelease(&sem->lock, irqState);
		};
	};

	// if there are no free semaphores, check if it was an interruption
	if (numFreeSems == 0)
	{
		if (_semIsInterrupted(flags))
		{
			return -EINTR;
		};
	};

	// otherwise return the number of free semaphores (0 correctly indicates a timeout)
	return numFreeSems;
};