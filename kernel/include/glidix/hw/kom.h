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

#ifndef __glidix_hw_kom_h
#define __glidix_hw_kom_h

#include <glidix/util/common.h>
#include <glidix/thread/spinlock.h>

/**
 * Types of pools.
 */
typedef enum
{
	KOM_POOL_UNUSED,
	KOM_POOL_PAGE_CACHE,
	KOM_POOL_INODES,

	KOM_NUM_POOLS,					// number of pools
} KOM_PoolType;

/**
 * Pool bits.
 */
#define	KOM_POOLBIT_UNUSED				(1 << 0)
#define	KOM_POOLBIT_PAGE_CACHE				(1 << 1)
#define	KOM_POOLBIT_INODES				(1 << 2)
#define	KOM_POOLBIT_ALL					((1 << KOM_NUM_POOLS) - 1)

/**
 * Given a bucket index, get the size of blocks stored in said bucket.
 */
#define	KOM_BUCKET_SIZE(bucketIndex)			(1UL << (6+(bucketIndex)))

/**
 * The bucket containing page-sized blocks.
 */
#define	KOM_BUCKET_PAGE					6

/**
 * Number of buckets in a pool.
 */
#define	KOM_NUM_BUCKETS					32

/**
 * Maximum number of regions.
 */
#define	KOM_MAX_REGIONS					64

/**
 * Kernel object header.
 */
typedef struct KOM_Header_ KOM_Header;
struct KOM_Header_
{
	KOM_Header* prev;
	KOM_Header* next;
};

/**
 * Pool of kernel objects.
 */
typedef struct
{
	/**
	 * The buckets. Each is a linked list of kernel objects, sorted by ascending
	 * address.
	 */
	KOM_Header* buckets[KOM_NUM_BUCKETS];
} KOM_Pool;

/**
 * User page information.
 */
typedef struct
{
	/**
	 * Reference count for this page.
	 */
	uint64_t refcount;

	/**
	 * Spinlock protecting the blocker list.
	 */
	Spinlock blockerLock;

	/**
	 * The blocker list.
	 */
	void *blockerList;
} KOM_UserPageInfo;

/**
 * Represents a region of memory.
 */
typedef struct
{
	/**
	 * Virtual base address.
	 */
	uint64_t virtualBase;

	/**
	 * Physical base address.
	 */
	uint64_t physBase;

	/**
	 * Size of the region in bytes.
	 */
	uint64_t size;

	/**
	 * Auxiliary information about the pages.
	 */
	KOM_UserPageInfo *pageInfo;
} KOM_Region;

/**
 * Intialize the Kernel Object Manager.
 */
void komInit();

/**
 * Allocate a block from the specified bucket, out of the allowed pools. The `allowedPools`
 * argument is a bitwise-OR of one or more of the `KOM_POOLBIT_*` macros, or `KOM_POOLBIT_ALL`
 * meaning any pool.
 * 
 * This will find a kernel object which is not currently in use, or free memory if possible,
 * and destroy (flush) the object, and return a pointer to it, which the user can destroy.
 * 
 * Use `komFreeBlock()` to release the block later.
 * 
 * Returns `NULL` if we are out of memory.
 */
void* komAllocBlock(int bucket, int allowedPools);

/**
 * Put the specified block in the specified bucket of the unused memory pool. This block
 * must have been previously allocated with `komAllocBlock()` from exactly the same bucket!
 */
void komReleaseBlock(void *block, int bucket);

/**
 * Allocate a page-aligned address, which will fit at least `size` bytes. This is used for
 * allocating virtual addresses statically; the returned address space can never be freed.
 */
void* komAllocVirtual(size_t size);

/**
 * Get the virtual address from a physical address previously returned by `komAllocBlock()`.
 * Returns NULL if the virtual address could not be found.
 */
void* komPhysToVirt(uint64_t phaddr);

/**
 * Get user page information for the specified pointer.
 */
KOM_UserPageInfo* komGetUserPageInfo(void *ptr);

/**
 * Allocate a user page, initially with a refcount of 1. Returns NULL if we have run out of
 * memory.
 */
void* komAllocUserPage();

/**
 * Decrement the refcount on a user page, and release it if the refcount becomes zero.
 */
void komUserPageUnref(void *page);

/**
 * Increment the refcount on a user page, and return the page again.
 */
void* komUserPageDup(void *page);

#endif