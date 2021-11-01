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

#ifndef __glidix_util_treemap_h
#define	__glidix_util_treemap_h

#include <glidix/util/common.h>
#include <glidix/thread/spinlock.h>
#include <glidix/util/errno.h>

/**
 * Number of children of each node.
 */
#define	TREEMAP_NUM_CHILDREN			256

/**
 * Depth of the map.
 */
#define	TREEMAP_DEPTH				4

typedef struct
{
	void* children[TREEMAP_NUM_CHILDREN];
} TreeMapNode;

/**
 * A data structure for storing ordered data with O(1) lookup, slightly slower
 * than an array, but with more optimal memory usage than arrays. Each TreeMap
 * can contain any index less than 2^32, even with sparse indices.
 * 
 * Maps integer indices to untyped pointers, `void*`. Nothing is done to the
 * values if the treemap is destroyed.
 * 
 * All treemap operations are NOT thread-safe. Access to a shared treemap must be
 * protected by a lock.
 */
typedef struct
{
	/**
	 * The master node.
	 */
	TreeMapNode masterNode;
} TreeMap;

/**
 * Create a new treemap. Make sure to destroy it later using `treemapDestroy()`.
 * 
 * Returns `NULL` if memory could not be allocated.
 */
TreeMap* treemapNew();

/**
 * Destroy a treemap.
 */
void treemapDestroy(TreeMap *map);

/**
 * Get the pointer at the specified index in the map. Returns NULL if the entry
 * doesn't exist.
 */
void* treemapGet(TreeMap *map, uint32_t index);

/**
 * Set a pointer at the specified index in the map. If the `ptr` is NULL, then
 * the entry is treated as deleted (as `treemapGet()` wil lbe returning NULL
 * for this entry).
 * 
 * Returns 0 on success, or an error number on error.
 * 
 * `ENOMEM` is returned if memory allocation wasn't possible.
 * 
 * If `old` is not NULL, then the old pointer is stored in it.
 */
errno_t treemapSet(TreeMap *map, uint32_t index, void *ptr);

#endif