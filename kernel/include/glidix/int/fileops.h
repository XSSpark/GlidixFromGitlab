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

#ifndef __glidix_int_fileops_h
#define	__glidix_int_fileops_h

#include <glidix/util/common.h>
#include <glidix/thread/process.h>

/**
 * Maximum size of I/O buffers.
 */
#define	SYS_FILEOP_BUFFER_MAX						0x7ffff000

/**
 * Implements the `openat()` system call.
 */
int sys_openat(int dirfd, user_addr_t upath, int oflags, mode_t mode);

/**
 * Implements the `close()` system call.
 */
int sys_close(int fd);

/**
 * Implements the `read()` system call.
 */
ssize_t sys_read(int fd, user_addr_t ubuffer, size_t size);

/**
 * Implements the `write()` system call.
 */
ssize_t sys_write(int fd, user_addr_t ubuffer, size_t size);

/**
 * Implements the `pread()` system call.
 */
ssize_t sys_pread(int fd, user_addr_t ubuffer, size_t size, off_t offset);

/**
 * Implements the `pwrite()` system call.
 */
ssize_t sys_pwrite(int fd, user_addr_t ubuffer, size_t size, off_t offset);

/**
 * Implements the `dup3()` system call (which is used to implement `dup()` and `dup2()`).
 * 
 * This is subtly different from the Linux `dup3()`. If `newfd` is -1, then a new file descriptor is allocated,
 * set to point to the same open file description as `oldfd`, and returned. If `newfd` is not -1, it must be a
 * file descriptor within the valid range; if the descriptor is in use, it will be closed and replaced with the
 * description of `oldfd`.
 * 
 * If `cloexec` is nonzero, then `newfd` will be automatically closed after a successful `exec*()`.
 * 
 * Returns the new (positive) file descriptor on success, or a negated error number on error. If `oldfd` and
 * `newfd` are the same value, returns `-EINVAL`.
 */
int sys_dup3(int oldfd, int newfd, int cloexec);

#endif