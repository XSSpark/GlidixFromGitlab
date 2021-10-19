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

#ifndef __glidix_thread_spinlock_h
#define	__glidix_thread_spinlock_h

#include <glidix/util/common.h>

/**
 * Represents a spinlock. This is a low-level synchronisation primitive, which synchronises
 * access to a resource between CPU cores.
 */
typedef struct
{
	int _;
} Spinlock;

/**
 * Represents the saved IRQ state when a spinlock is acquired.
 */
typedef uint64_t SpinIrqState;

/**
 * Acquire a spinlock. This function disables interrupts, then loops until it can take the
 * spinlock. Returns the previous IRQ state, which must later be passed to `spinlockRelease()`.
 */
SpinIrqState spinlockAcquire(Spinlock *sl);

/**
 * Release a spinlock. This function must only be called by the thread which holds the spinlock
 * currently. The `irqState` is the value returned by `spinlockAcquire()` previously.
 */
void spinlockRelease(Spinlock *sl, SpinIrqState irqState);

#endif