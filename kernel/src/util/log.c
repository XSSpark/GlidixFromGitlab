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

#include <glidix/util/log.h>
#include <glidix/display/console.h>
#include <glidix/util/format.h>
#include <glidix/hw/port.h>

/**
 * Size of the buffer used for string formatting.
 */
#define	FORMAT_BUFFER_SIZE						2046

static void kputE9(const char *str)
{
	while (*str != 0) outb(0xE9, *str++);
};

void kputlog(const char *str)
{
	// TODO: log targets other than the console
	kputE9(str);
	conWriteString(str);
};

void kvprintf(const char *fmt, va_list ap)
{
	char buffer[FORMAT_BUFFER_SIZE];
	kvsnprintf(buffer, FORMAT_BUFFER_SIZE, fmt, ap);
	kputlog(buffer);
};

void kprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	kvprintf(fmt, ap);
	va_end(ap);
};