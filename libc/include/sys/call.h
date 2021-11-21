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

#ifndef _SYS_CALL_H
#define _SYS_CALL_H

#include <inttypes.h>

#define __SYS_exit							0
#define	__SYS_sigaction							1
#define	__SYS_sigmask							2
#define	__SYS_fork							3
#define	__SYS_openat							4
#define	__SYS_close							5
#define	__SYS_read							6
#define	__SYS_write							7
#define	__SYS_pread							8
#define	__SYS_pwrite							9
#define	__SYS_getpid							10
#define	__SYS_getppid							11
#define	__SYS_waitpid							12
#define	__SYS_setsid							13
#define	__SYS_getsid							14
#define	__SYS_setpgid							15
#define	__SYS_getpgrp							16
#define	__SYS_kill							17
#define	__SYS_dup3							18
#define	__SYS_pthread_self						19
#define	__SYS_raise							20
#define	__SYS_mmap							21
#define	__SYS_thexit							22
#define	__SYS_munmap							23
#define	__SYS_mprotect							24
#define	__SYS_pthread_detach						25

// TODO
#define	__SYS_block_on							255
#define	__SYS_unblock							255
#define	__SYS_sockerr							255
#define	__SYS_utime							255
#define	__SYS_getdent							255
#define	__SYS_getktu							255
#define	__SYS_readlink							255
#define	__SYS_getcwd							255
#define	__SYS_fcntl_getfl						255
#define	__SYS_fcntl_setfl						255
#define	__SYS_flock_get							255
#define	__SYS_flock_set							255
#define	__SYS_mv							255
#define	__SYS_realpath							255

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Invoke a system call.
 */
uint64_t __syscall(int __sysno, ...);

#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif	/* _SYS_CALL_H */
