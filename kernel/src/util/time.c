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

#include <glidix/util/time.h>
#include <glidix/thread/spinlock.h>

/**
 * The number of nanoseconds we've been up for.
 */
static volatile nanoseconds_t uptime;

/**
 * The spinlock protecting the timed event list.
 */
static Spinlock timedLock;

/**
 * List of timed events, ordered by closest deadline first.
 */
static TimedEvent *timedHead;

nanoseconds_t timeGetUptime()
{
	return uptime;
};

void timeIncrease(nanoseconds_t nanos)
{
	__sync_fetch_and_add(&uptime, nanos);

	SpinIrqState irqState = spinlockAcquire(&timedLock);
	while (timedHead != NULL && timedHead->deadline <= uptime)
	{
		TimedEvent *timed = timedHead;
		timedHead = timed->next;
		if (timedHead != NULL) timedHead->prev = NULL;

		timed->isCancelled = 1;
		schedWake(timed->waiter);
	};
	spinlockRelease(&timedLock, irqState);
};

void timedPost(TimedEvent *timed, nanoseconds_t deadline)
{
	Thread *me = schedGetCurrentThread();
	SpinIrqState irqState = spinlockAcquire(&timedLock);

	timed->deadline = deadline;
	if (deadline <= uptime)
	{
		timed->isCancelled = 1;
		spinlockRelease(&timedLock, irqState);
		return;
	};

	timed->waiter = me;
	timed->isCancelled = 0;

	// add us to the queue
	if (timedHead == NULL || timedHead->deadline > deadline)
	{
		timed->next = timedHead;
		timed->prev = NULL;
		if (timedHead != NULL) timedHead->prev = timed;
		timedHead = timed;
	}
	else
	{
		TimedEvent *prev = timedHead;
		while (prev->next != NULL && timedHead->deadline < deadline)
		{
			prev = prev->next;
		};

		timed->prev = prev;
		timed->next = prev->next;

		if (prev->next != NULL) prev->next->prev = timed;
		prev->next = timed;
	};

	spinlockRelease(&timedLock, irqState);
};

void timedCancel(TimedEvent *timed)
{
	SpinIrqState irqState = spinlockAcquire(&timedLock);

	if (!timed->isCancelled)
	{
		timed->isCancelled = 1;

		if (timed->prev != NULL) timed->prev->next = timed->next;
		if (timed->next != NULL) timed->next->prev = timed->prev;
		if (timedHead == timed) timedHead = timed->next;
	};

	spinlockRelease(&timedLock, irqState);
};

void timeSleep(nanoseconds_t nanos)
{
	nanoseconds_t deadline = uptime + nanos;

	TimedEvent timed;
	timedPost(&timed, deadline);

	while (uptime < deadline)
	{
		schedSuspend();
	};

	timedCancel(&timed);
};