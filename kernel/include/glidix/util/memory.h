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

#ifndef __glidix_util_memory_h
#define	__glidix_util_memory_h

#include <glidix/util/common.h>

/**
 * Header which comes before the pointers returned by `kmalloc()`.
 */
typedef union
{
	struct
	{
		/**
		 * The bucket from which this block came.
		 */
		int bucket;

		/**
		 * Actual size of the block as given to `kmalloc()`/`krealloc()`.
		 */
		size_t actualSize;
	};

	/**
	 * Force this structure to be 16 bytes long (so that the data after the header
	 * is aligned).
	 */
	char pad[16];
} HeapHeader;

/**
 * Allocate a block of `size` bytes. Returns `NULL` if it was not possible to
 * allocate a block of this size.
 */
void* kmalloc(size_t size);

/**
 * Change the size of `block` to `size`; the original pointer becomes invalid after
 * this call, and the new pointer to use is returned; this is the kernel equivalent
 * of `realloc()`.
 */
void* krealloc(void *block, size_t size);

/**
 * Free a pointer previously returned by `kmalloc()` or `krealloc()`.
 */
void kfree(void *ptr);

#endif