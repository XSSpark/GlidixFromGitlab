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

#include <glidix/thread/mutex.h>
#include <glidix/util/string.h>
#include <glidix/util/panic.h>

void mutexInit(Mutex *mtx)
{
	memset(mtx, 0, sizeof(Mutex));
};

void mutexLock(Mutex *mtx)
{
	Thread *me = schedGetCurrentThread();
	IrqState irqState = spinlockAcquire(&mtx->lock);

	if (irqState == IRQ_STATE_DISABLED)
	{
		panic("mutexLock was called with interrupts disabled!");
	};

	if (mtx->owner == me)
	{
		// already the owner, increment lock count
		mtx->numLocks++;
	}
	else if (mtx->owner == NULL)
	{
		// no owner, so become the owner
		mtx->owner = me;
		mtx->numLocks = 1;
	}
	else
	{
		// initialize our waiter struct
		MutexWaiter waiter;
		waiter.thread = me;
		waiter.next = NULL;

		if (mtx->last == NULL)
		{
			mtx->first = mtx->last = &waiter;
		}
		else
		{
			mtx->last->next = &waiter;
			mtx->last = &waiter;
		};

		// when the previous owner calls mtxUnlock(), they will remove us
		// from the queue and make us the owner and set numLocks to 1
		while (mtx->owner != me)
		{
			spinlockRelease(&mtx->lock, irqState);
			schedSuspend();
			irqState = spinlockAcquire(&mtx->lock);
		};
	};

	spinlockRelease(&mtx->lock, irqState);
};

int mutexTryLock(Mutex *mtx)
{
	Thread *me = schedGetCurrentThread();
	IrqState irqState = spinlockAcquire(&mtx->lock);

	if (irqState == IRQ_STATE_DISABLED)
	{
		panic("mutexLock was called with interrupts disabled!");
	};

	int status = 0;
	if (mtx->owner == me)
	{
		// i am the owner, increase lock count
		mtx->numLocks++;
	}
	else if (mtx->owner == NULL)
	{
		// nobody is the owner, acquire it
		mtx->owner = me;
		mtx->numLocks = 1;
	}
	else
	{
		// some other thread owns it, return failure
		status = -1;
	};

	spinlockRelease(&mtx->lock, irqState);
	return status;
};

void mutexUnlock(Mutex *mtx)
{
	Thread *me = schedGetCurrentThread();
	IrqState irqState = spinlockAcquire(&mtx->lock);
	
	if (mtx->owner != me)
	{
		panic("Attempted to unlock a mutex which you are not holding!");
	};

	if (--mtx->numLocks == 0)
	{
		// last lock was released
		mtx->owner = NULL;

		// if any threads are waiting, pass it to the next one
		if (mtx->first != NULL)
		{
			mtx->owner = mtx->first->thread;
			schedWake(mtx->owner);
			mtx->numLocks = 1;
			mtx->first = mtx->first->next;
			if (mtx->first == NULL) mtx->last = NULL;
		};
	};

	spinlockRelease(&mtx->lock, irqState);
};