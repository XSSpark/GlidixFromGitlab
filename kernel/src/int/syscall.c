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

#include <glidix/int/syscall.h>
#include <glidix/thread/sched.h>
#include <glidix/util/panic.h>
#include <glidix/int/exit.h>
#include <glidix/util/string.h>
#include <glidix/int/procops.h>

/**
 * The system call table. This must not be static, as it must be accessed by `syscall.asm`!
 * An entry is allowed to be NULL, to specify an invalid system call. Please ensure that the
 * system calls are numbered correctly in the comments next to them.
 */
void* _sysCallTable[] = {
	sys_exit,							// 0
	sys_sigaction,							// 1
	sys_sigmask,							// 2
	sys_fork,							// 3
};

/**
 * Export the number of system calls, so that `syscall.asm` can access it.
 */
uint64_t _sysCallCount = sizeof(_sysCallTable)/sizeof(void*);

/**
 * This is called when an invalid syscall is detected.
 */
void _sysCallInvalid()
{
	ksiginfo_t si;
	memset(&si, 0, sizeof(ksiginfo_t));

	si.si_signo = SIGSYS;
	sysDispatchSignal(&si, (uint64_t) -ENOSYS);
};

void sysDispatchSignal(ksiginfo_t *si, uint64_t rax)
{
	SyscallContext *ctx = schedGetCurrentThread()->syscallContext;

	kmcontext_gpr_t gprs;
	memset(&gprs, 0, sizeof(gprs));

	gprs.rax = rax;
	gprs.rbx = ctx->rbx;
	gprs.rbp = ctx->rbp;
	gprs.rsp = ctx->rsp;
	gprs.rflags = ctx->rflags;
	gprs.r12 = ctx->r12;
	gprs.r13 = ctx->r13;
	gprs.r14 = ctx->r14;
	gprs.r15 = ctx->r15;
	gprs.rip = ctx->rip;

	schedDispatchSignal(&gprs, &ctx->fpuRegs, si);
};

/**
 * Check for signals, and dispatch them (with `rax` value on return) if there are any; otherwise,
 * simply return `rax`.
 */
uint64_t _sysCheckSignals(uint64_t rax)
{
	ksiginfo_t si;
	if (schedCheckSignals(&si) == 0)
	{
		sysDispatchSignal(&si, rax);
	};

	return rax;
};