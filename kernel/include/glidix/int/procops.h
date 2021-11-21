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

#ifndef __glidix_int_procops_h
#define	__glidix_int_procops_h

#include <glidix/util/common.h>
#include <glidix/thread/process.h>

/**
 * Fork the current process. Returns 0 to the child, the child's (positive) PID to the
 * parent; or, if an error occured, no process is created, and returns a negated error
 * number.
 */
pid_t sys_fork();

/**
 * Get the PID of the calling process.
 */
pid_t sys_getpid();

/**
 * Get the parent process ID.
 */
pid_t sys_getppid();

/**
 * Wait for a child to terminate (implements the `waitpid()` syscall).
 */
pid_t sys_waitpid(pid_t pid, user_addr_t uwstatus, int flags);

/**
 * Create a new session. Implements the `setsid()` system call.
 */
int sys_setsid();

/**
 * Get the session ID.
 */
pid_t sys_getsid();

/**
 * Implements the `setpgid()` system call.
 */
int sys_setpgid(pid_t pid, pid_t pgid);

/**
 * Get the process group ID of the calling process.
 */
pid_t sys_getpgrp();

/**
 * Send a signal to a process or processes.
 */
int sys_kill(pid_t pid, int signo);

/**
 * Get the thread ID of the calling process.
 */
thid_t sys_pthread_self();

/**
 * Raise a signal. Returns 0 on success, negated error number on error.
 */
int sys_raise(int signo);

/**
 * Exit from the current userspace thread.
 */
void sys_thexit(thretval_t retval);

/**
 * Unmap address space. Returns 0 on success, or a negated error number on error.
 */
int sys_munmap(user_addr_t addr, size_t len);

/**
 * Set the protection on address space. Returns 0 on success, or a negated error number on error.
 */
int sys_mprotect(user_addr_t addr, size_t len, int prot);

/**
 * Detach the specified thread ID. Returns 0 on success, or an error number on error.
 */
errno_t sys_pthread_detach(thid_t thid);

#endif