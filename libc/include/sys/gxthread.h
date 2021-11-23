/*
	Glidix Standard C Library (libc)
	
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

#ifndef _SYS_GXTHREAD_H
#define	_SYS_GXTHREAD_H

#include <stdint.h>

#define	__THWAIT_EQUALS							0
#define	__THWAIT_NEQUALS						1

/**
 * Represents a thread ID (equivalent to `pthread_t`).
 */
typedef int __thid_t;

/**
 * Exit from the current thread, returning the specified value. This bypasses any
 * `pthread_atexit` handlers.
 */
_Noreturn void __thexit(void *retval);

/**
 * Wait for the value pointed to be `ptr` to equal `expectedValue`. Before calling,
 * check if `ptr` already points to `expectedValue`. Make sure that whenever the value
 * at `ptr` is changed, `__thsignal()` is called to notify any threads blocking on
 * this value. Returns 0 on success, or an error number on error. This function can
 * return false positives; i.e. a 0 when `ptr` still does not equal `expectedValue`
 * (this can be due to a race or due to being interrupted by signals); so you have to
 * call it in a loop, checking the condition every time. The following errors are possible:
 * 
 * `EINVAL` - the address is not aligned
 * `EFAULT` - the address is not mapped as read/write
 */
int __thwait(volatile uint64_t *ptr, int op, uint64_t expectedValue);

/**
 * Inform any threads waiting on `ptr` to equal `newValue`, that the change has been made.
 * Call function after you've actually set it to `newValue`! Returns 0 on success, or an
 * error number on error; the following errors are possible:
 * 
 * `EINVAL` - the address is not aligned
 * `EFAULT` - the address is not mapped as read/write
 */
int __thsignal(volatile uint64_t *ptr, uint64_t newValue);

#ifdef _GLIDIX_SOURCE
#define	thexit __thexit
#define	thid_t __thid_t
#define	thwait __thwait
#define	thsignal __thsignal
#define	THWAIT_EQUALS __THWAIT_EQUALS
#define	THWAIT_NEQUALS __THWAIT_NEQUALS
#endif

#endif