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

#ifndef __glidix_int_thwait_h
#define	__glidix_int_thwait_h

#include <glidix/int/syscall.h>
#include <glidix/thread/sched.h>
#include <glidix/thread/process.h>

/**
 * Wait conditions.
 */
#define	THWAIT_EQUALS							0
#define	THWAIT_NEQUALS							1

/**
 * An entry in the blocker list of a page.
 */
typedef struct Blocker_ Blocker;
struct Blocker_
{
	/**
	 * The previous blocker.
	 */
	Blocker *prev;
	
	/**
	 * The next blocker.
	 */
	Blocker *next;

	/**
	 * Offset into the page.
	 */
	uint64_t offset;

	/**
	 * The thread waiting on this offset.
	 */
	Thread *waiter;

	/**
	 * What value this thread is waiting for.
	 */
	uint64_t compareValue;
};

/**
 * Suspends the calling thread atomically if a comparison of the value at the specified location with
 * `compare` matches the `op`. The pointer must be 8-byte-aligned and on a writeable page. This function
 * returns 0 if the condition was met, OR if we were interrupted by a signal. An error number is returned
 * on error:
 * 
 * `EINVAL` - the pointer is misaligned or `op` is invalid
 * `EFAULT` - the pointer is not writeable
 * 
 * The valid operations (`op`) are:
 * 
 * `THWAIT_EQUALS` - wait until the value equals `compare`.
 * `THWAIT_NEQUALS` - wait until the value does not equal `compare`.
 * 
 * Please note that this function will NOT be checking the value repeatedly when suspended. To ensure that
 * threads are woken up, whenever you update the value, you must call `sys_thsignal()` to run the checks
 * on the value and wake threads.
 */
errno_t sys_thwait(user_addr_t uptr, int op, uint64_t compare);

/**
 * Unblock threads waiting for the specified value at the specified address. Returns 0 on success, or an
 * error number on error; an error occurs if the address is not aligned or not mapped.
 */
errno_t sys_thsignal(user_addr_t uptr, uint64_t newValue);

#endif