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

#ifndef __glidix_util_time_h
#define	__glidix_util_time_h

#include <glidix/util/common.h>
#include <glidix/thread/sched.h>

/**
 * Number of nanoseconds per second.
 */
#define	NANOS_PER_SEC				1000000000UL

/**
 * Make a `nanoseconds_t` given a number of seconds.
 */
#define	TIME_SEC(s)				(NANOS_PER_SEC*(s))

/**
 * Make a `nanoseconds_t` given a number of milliseconds.
 */
#define	TIME_MILLI(m)				(1000000UL*(m))

/**
 * Make a `nanoseconds_t` given a number of microseconds.
 */
#define	TIME_MICRO(u)				(1000UL*(u))

/**
 * Specifies a number of nanoseconds.
 */
typedef uint64_t nanoseconds_t;

/**
 * Represents a thread to be woken up at a specific time.
 * 
 * This structure can be allocated on the stack of a thread. Initialize it
 * by calling `timedPost()`, which will both initialize it and also add it
 * to the timed event queue. Keep suspending in a loop until the deadline
 * is reached or if you want to wake up for some other reason.
 * 
 * Finally, REGARDLESS of whether the thread was woken up by the event, or
 * by some other way, call `timedCancel()` to clean up, before deallocating
 * the structure.
 */
typedef struct TimedEvent_ TimedEvent;
struct TimedEvent_
{
	/**
	 * The deadline (at which the thread will be woken).
	 */
	nanoseconds_t deadline;

	/**
	 * The thread to be woken up.
	 */
	Thread *waiter;

	/**
	 * Links.
	 */
	TimedEvent *prev;
	TimedEvent *next;

	/**
	 * Has the event been cancelled?
	 */
	int isCancelled;
};

/**
 * Get the kernel's uptime; this is the number of nanoseconds which passed since the
 * clock was initialized.
 */
nanoseconds_t timeGetUptime();

/**
 * Increase the uptime by the specified number of nanoseconds. This is usually called
 * from a timer interrupt handler, and is async-interrupt-safe.
 */
void timeIncrease(nanoseconds_t nanos);

/**
 * Add a new timed event to the list, to wake up the calling thread at the specified
 * deadline.
 * 
 * This initializes the structure (which may be allocated on the stack), and adds it
 * to the list. When done with it, call `timedCancel()` (regardless of whether the
 * deadline was reached or not).
 * 
 * See `TimedEvent` documentation for more information.
 */
void timedPost(TimedEvent *timed, nanoseconds_t deadline);

/**
 * Remove the timed event from the list. It is allowed to be called multiple times.
 */
void timedCancel(TimedEvent *timed);

/**
 * Sleep for the specified number of nanoseconds. This is intended for use by kernel
 * threads, and it will ignore signals while waiting!
 */
void timeSleep(nanoseconds_t nanos);

#endif