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

#ifndef __glidix_thread_mutex_h
#define	__glidix_thread_mutex_h

#include <glidix/util/common.h>
#include <glidix/thread/sched.h>
#include <glidix/thread/spinlock.h>

/**
 * Represents an entry in a mutex's queue.
 */
typedef struct MutexWaiter_ MutexWaiter;
struct MutexWaiter_
{
	Thread *thread;
	MutexWaiter *next;
};

/**
 * Represents a recursive mutex.
 * 
 * Note that a mutex initialized to all zeroes is valid!
 */
typedef struct 
{
	/**
	 * The spinlock which protects this mutex.
	 */
	Spinlock lock;

	/**
	 * Current owner of this mutex (or NULL if unlocked).
	 */
	Thread *owner;

	/**
	 * Number of times the current thread has locked the mutex.
	 */
	int numLocks;

	/**
	 * Queue of threads waiting for this mutex.
	 */
	MutexWaiter *first;
	MutexWaiter *last;
} Mutex;

/**
 * Initialize the mutex `mtx`. This must be done once, before any synchronization
 * begins. Note that this is equivalent to simply zeroing out the mutex!
 */
void mutexInit(Mutex *mtx);

/**
 * Lock the mutex. If the mutex is currently owned by another thread, this blocks
 * until the mutex is free. If this mutex is currently owned by the calling thread,
 * then its 'lock count' increases; you must call `mutexUnlock()` the same number of
 * times to actually unlock it.
 */
void mutexLock(Mutex *mtx);

/**
 * Attempt to lock a mutex. See `mutexLock()` for explanation of the semantics. The
 * only difference is that if `mutexLock()` would have blocked, this function returns
 * `-1` and does not try to lock the mutex; otherwise, the semantics are the same
 * and this function returns `0`.
 */
int mutexTryLock(Mutex *mtx);

/**
 * Unlock the mutex. Note that the calling thread may have locked this mutex multiple
 * times recursively; the mutex is only unlocked once the same number of calls was
 * made to `mutexUnlock()` as `mutexLock()`.
 */
void mutexUnlock(Mutex *mtx);

#endif