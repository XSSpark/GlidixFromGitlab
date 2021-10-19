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

#ifndef __glidix_hw_msr_h
#define	__glidix_hw_msr_h

#include <glidix/util/common.h>

/**
 * MSR numbers.
 */
#define	MSR_STAR			0xC0000081
#define	MSR_LSTAR			0xC0000082
#define	MSR_CSTAR			0xC0000083
#define	MSR_SFMASK			0xC0000084
#define	MSR_EFER			0xC0000080
#define	MSR_KERNEL_GS_BASE		0xC0000102
#define MSR_FS_BASE			0xC0000100
#define MSR_GS_BASE			0xC0000101

/**
 * EFER bits.
 */
#define	EFER_SCE			(1 << 0)		/* system call extensions */
#define	EFER_NXE			(1 << 11)		/* NX enable */

/**
 * Write `value` to the specified MSR.
 */
static inline void wrmsr(uint64_t msr, uint64_t value)
{
	uint32_t low = value & 0xFFFFFFFF;
	uint32_t high = value >> 32;
	ASM (
		"wrmsr"
		:
		: "c"(msr), "a"(low), "d"(high)
	);
};

/**
 * Read the value of an MSR.
 */
static inline uint64_t rdmsr(uint64_t msr)
{
	uint32_t low, high;
	ASM (
		"rdmsr"
		: "=a"(low), "=d"(high)
		: "c"(msr)
	);
	return ((uint64_t)high << 32) | low;
};

#endif