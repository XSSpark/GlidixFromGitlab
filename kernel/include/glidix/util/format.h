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

#ifndef __glidix_util_format_h
#define	__glidix_util_format_h

#include <glidix/util/common.h>

/**
 * "Print" a formatted string into the speicfied `buffer`, of size `size`. The size includes
 * the final NUL character. If the string overflows the buffer, the end is truncated, and a
 * NUL character is still inserted. The length of the resulting string (excluding the NUL,
 * and including any truncated bytes) is returned.
 */
size_t kvsnprintf(char *buffer, size_t size, const char *fmt, va_list ap);

/**
 * "Print" a formatted string into the speicfied `buffer`, of size `size`. The size includes
 * the final NUL character. If the string overflows the buffer, the end is truncated, and a
 * NUL character is still inserted. The length of the resulting string (excluding the NUL,
 * and including any truncated bytes) is returned.
 */
size_t ksnprintf(char *buffer, size_t size, const char *fmt, ...) FORMAT(printf, 3, 4);

#endif