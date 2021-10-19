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

#ifndef __glidix_hw_port_h
#define	__glidix_hw_port_h

#include <glidix/util/common.h>

static inline uint8_t inb(uint16_t port)
{
	uint8_t result;
	ASM ("inb %1, %0" : "=a" (result) : "dN" (port));
	return result;
};

static inline uint16_t inw(uint16_t port)
{
	uint16_t result;
	ASM ("inw %1, %0" : "=a" (result) : "dN" (port));
	return result;
};

static inline uint32_t ind(uint16_t port)
{
	uint32_t result;
	ASM ("inl %1, %0" : "=a" (result) : "dN" (port));
	return result;
};

static inline void outb(uint16_t port, uint8_t value)
{
	ASM ("outb %1, %0" : : "dN" (port), "a" (value));
};

static inline void outw(uint16_t port, uint16_t value)
{
	ASM ("outw %1, %0" : : "dN" (port), "a" (value));
};

static inline void outd(uint16_t port, uint16_t value)
{
	ASM ("outl %1, %0" : : "dN" (port), "a" (value));
};

#endif