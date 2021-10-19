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

#ifndef __glidix_util_common_h
#define	__glidix_util_common_h

// ALL freestanding headers used by the kernel should be included from here,
// and other parts of the kernel should only use them via this header, for
// abstraction purposes.
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#define	ASM					__asm__ __volatile__
#define	ALIGN(n)				__attribute__ ((aligned(n)))
#define	PACKED					__attribute__ ((packed))
#define	cli()					ASM ("cli")
#define	sti()					ASM ("sti")
#define	hlt()					ASM ("hlt")
#define	kalloca					__builtin_alloca
#define	FORMAT(a, b, c)				__attribute__ ((format(a, b, c)))
#define	noreturn				_Noreturn

#ifndef	__SYSTYPES_DEFINED
#define	__SYSTYPES_DEFINED
typedef int64_t					ssize_t;
typedef	int					pid_t;
typedef	uint64_t				uid_t;
typedef	uint64_t				gid_t;
#endif

#endif