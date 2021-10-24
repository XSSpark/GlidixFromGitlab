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

#ifndef __glidix_thread_semaphore_h
#define	__glidix_thread_semaphore_h

#include <glidix/util/common.h>
#include <glidix/thread/spinlock.h>
#include <glidix/thread/sched.h>
#include <glidix/util/time.h>

/**
 * When passed as a flag to `semWaitGen()`, causes it to return `-EINTR` if a signal arrives
 * before resources become available.
 */
#define	SEM_W_INTR				(1 << 0)

/**
 * When passed as a flag to `semWaitGen()`, causes it to return `-EAGAIN` if no resources are
 * available immediately.
 *
 * NOTE: This MUST have the same value as `O_NONBLOCK`.
 */
#define	SEM_W_NONBLOCK				(1 << 8)

/**
 * This macro converts file descriptor flags (`O_*`) into flags appropriate for `semWaitGen()`:
 * That is, `SEM_W_INTR`, and if `O_NONBLOCK` is passed, `SEM_W_NONBLOCK`.
 */
#define	SEM_W_FILE(oflag)			(((oflag) & (SEM_W_NONBLOCK)) | SEM_W_INTR)

/**
 * Semaphore flag indicating the semaphore has been terminated.
 */
#define	SEM_TERMINATED				(1 << 0)

/**
 * Maximum number of semaphores we can poll using `semPoll()`.
 */
#define	SEM_POLL_MAX				1024

/**
 * Represents an entry in a semaphore's queue.
 */
typedef struct SemWaiter_ SemWaiter;
struct SemWaiter_
{
	/**
	 * The thread to be woken up when resources are available.
	 */
	Thread *thread;

	/**
	 * Number of units requested by this waiter.
	 */
	int requested;

	/**
	 * This is initialized to 0 by the waiting thread, and set to the number of units
	 * given when signalled.
	 */
	int given;

	/**
	 * Initialized to 0 by waiting thread, set to 1 when we are supposed to wake up,
	 * and `given` is set correctly.
	 */
	int signalled;

	/**
	 * Links.
	 */
	SemWaiter *prev;
	SemWaiter *next;
};

/**
 * Represents a semaphore. This structure can be allocated either on the stack or the heap,
 * and must be initialized using `semInit()` or `semInit2()` before any concurrency happens.
 */
typedef struct
{
	/**
	 * The lock protecting this semaphore.
	 */
	Spinlock lock;

	/**
	 * Number of resources currently available. -1 means terminated and empty.
	 */
	int count;

	/**
	 * Semaphore flags.
	 */
	int flags;

	/**
	 * Queue of threads waiting for resources.
	 */
	SemWaiter *first;
	SemWaiter *last;
} Semaphore;

/**
 * Initialize the semaphore with 1 unit.
 */
void semInit(Semaphore *sem);

/**
 * Initialize the semaphore with `count` resources.
 */
void semInit2(Semaphore *sem, int count);

/**
 * Generic semaphore waiting function; used to implement all of the wait functions below. `sem` names the
 * semaphore to wait on. `count` is the maximum number of resources to wait for. `flags` describes how to
 * wait (see the `SEM_W_*` macros above). `nanotimeout` is the maximum amount of time, in nanoseconds, that
 * we should try waiting; 0 means infinity.
 *
 * If `count` is 0, the function returns `-EAGAIN`. If it is -1, this function returns all available resources;
 * or `-EAGAIN` if none are available.
 *
 * Returns the number of acquired resources on success (which may be zero if the semaphore was terminated
 * with a call to `semTerminate()`). It returns an error number converted to negative on error; for example
 * `-EINTR`. Possible errors:
 *
 * `EINTR`	The `SEM_W_INTR` flag was passed, and a signal arrived before any resources became
 *		available.
 * `ETIMEDOUT`	A timeout was given and the time passed before any resources became available.
 * `EAGAIN`	The `SEM_W_NONBLOCK` flag was passed, and no resources are currently available.
 *
 * When waiting on a file read semaphore, the file desciptor flags (`fp->oflag`) must be taken into account,
 * and the function can be called as follows:
 *	`int count = semWaitGen(semRead, (int) readSize, SEM_W_FILE(fp->oflag), timeout);`
 * Which makes the function interruptable (`SEM_W_INTR`) and nonblocking if the `O_NONBLOCK` flag is in `oflag`.
 */
int semWaitGen(Semaphore *sem, int count, int flags, nanoseconds_t nanotimeout);

/**
 * Wait for exactly 1 resource to become available. Do not call this on semaphores that can be terminated, as
 * that will cause a kernel panic; this one is intended to make the semaphore useable as a lock.
 */
void semWait(Semaphore *sem);

/**
 * Add one resource to the semaphore.
 */
void semSignal(Semaphore *sem);

/**
 * Add the specified number of resources to the semaphore.
 */
void semSignal2(Semaphore *sem, int count);

/**
 * Terminate the semaphore. It is not valid to signal it anymore, and any thread waiting on this semaphore either
 * now or in the future, will receive 0 resources without blocking. This is used to signal ends of streams, e.g.
 * when the semaphore is used for an input pipe.
 */
void semTerminate(Semaphore *sem);

/**
 * Poll a group of semaphores. `numSems` specifies the number of semaphores in the array; `sems` is an array of
 * pointers to semaphores. `bitmap` points to a bitmap of results; this function never clears any bits, only sets
 * them if necessary. `flags` and `nanotimeout` are the same as for `semWaitGen()`.
 *
 * This function waits for at least one semaphore in the list to become available (have a nonzero count). If
 * `SEM_W_INTR` is set, and NONE of the semaphores become available before delivery of a signal, returns `-EINTR`.
 * If `SEM_W_NONBLOCK` is set, and NONE of the semaphores are immediately available, returns 0. On success,
 * returns the number of semaphores that became free; 0 meaning the operation timed out before any semaphores
 * became free.
 *
 * For each semaphore that is free, this function sets a bit in the `bitmap` before returning; that is, if
 * sems[n] become free, then testing the mask (1 << (n%8)) against bitmap[n/8] will return nonzero.
 *
 * This function does NOT change the count of any of the semaphores; you must still call `semWaitGen()` or `semWait()`
 * on a free semaphore to acquire it.
 *
 * A `NULL` pointer indicates a semaphore which NEVER becomes free.
 *
 * A terminated semaphore is considered free.
 *
 * Note that false positives are possible: for example, there may be a race condition where another thread acquires
 * all resources of a semaphore after `semPoll()` reported the semaphore free, but before the caller of `semPoll()`
 * called `semWait()` or `semWaitGen()`. To handle this case, `semWaitGen()` must be called with the `SEM_W_NONBLOCK`
 * flag.
 * 
 * If `numSems` is more than `SEM_POLL_MAX`, this function immediately returns `-EINVAL`.
 */
int semPoll(int numSems, Semaphore **sems, uint8_t *bitmap, int flags, nanoseconds_t nanotimeout);

#endif