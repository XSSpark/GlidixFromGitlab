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

#include <glidix/hw/kom.h>
#include <glidix/util/init.h>
#include <glidix/util/log.h>
#include <glidix/hw/pagetab.h>
#include <glidix/util/string.h>
#include <glidix/thread/spinlock.h>

/**
 * The allocator lock.
 */
static Spinlock komLock;

/**
 * The array of pools.
 */
static KOM_Pool komPools[KOM_NUM_POOLS];

/**
 * This is defined in the linker script, `kernel.ld`, to be a page-aligned address
 * past the end of the kernel address space. We use this to allocate virtual addresses
 * for the physical memory map.
 */
extern char __virtMapArea[];

/**
 * The next virtual address to return for virtual allocations.
 */
static char* nextVirtualAddr;

static void _komReleaseIntoPool(KOM_Pool *pool, KOM_Header *obj, int bucketIndex);

static uint64_t placementAlloc(uint64_t *placeptr)
{
	uint64_t result = *placeptr;
	(*placeptr) += PAGE_SIZE;
	return result;
};

void komInit()
{
	uint64_t mmap = (uint64_t) bootInfo->mmap;
	uint64_t mmapEnd = mmap + bootInfo->mmapSize;
	kprintf("Virtual mapping area begins at: 0x%p\n", __virtMapArea);

	uint64_t place = bootInfo->end;
	place = (place + 0xFFF) & ~0xFFF;
	kprintf("Physical placement of page tables will begin at: 0x%lx\n", place);

	kprintf("Creating page tables for useable memory...\n");
	kprintf("%-21s%s\n", "Phys. addr", "Size (bytes)");

	char *vaddr = __virtMapArea;
	while (mmap < mmapEnd)
	{
		MemoryMapEntry *ent = (MemoryMapEntry*) mmap;

		if (ent->type == 1 && (ent->baseAddr & 0xFFF) == 0)
		{
			kprintf("0x%016lx   0x%lx\n", ent->baseAddr, ent->len);

			uint64_t phaddr;
			for (phaddr=ent->baseAddr; phaddr<ent->baseAddr+ent->len; phaddr+=512*PAGE_SIZE)
			{
				if (phaddr >= place)
				{
					PageNodeEntry *nodes[4];
					pagetabGetNodes(vaddr, nodes);

					// map the first 3 levels of the page table
					int i;
					for (i=0; i<3; i++)
					{
						if ((nodes[i]->value & PT_PRESENT) == 0)
						{
							nodes[i]->value = placementAlloc(&place) | PT_PRESENT | PT_WRITE | PT_NOEXEC;
							invlpg(nodes[i+1]);
							memset(pagetabGetPageStart(nodes[i+1]), 0, PAGE_SIZE);
						};

						__sync_synchronize();
					};

					vaddr += 512*PAGE_SIZE;
				};
			};
		};

		mmap += ent->size + 4;
	};

	kprintf("\nFinal start of useable physical memory: 0x%lx\n", place);
	kprintf("Mapping physical memory:\n");
	kprintf("%-21s%-21s%s\n", "Virt. addr", "Phys. addr", "Size (bytes)");

	// the virtual addresses of the PTEs are consecutive, so we can use an iterator
	// to map the pages
	vaddr = __virtMapArea;
	uint64_t *pte = ((uint64_t*)(((((uint64_t)vaddr) >> 9) & (~(0x7UL | 0xFFFF000000000000UL))) | 0xFFFFFF8000000000));
	mmap = (uint64_t) bootInfo->mmap;

	while (mmap < mmapEnd)
	{
		MemoryMapEntry *ent = (MemoryMapEntry*) mmap;

		if (ent->type == 1 && (ent->baseAddr & 0xFFF) == 0)
		{
			uint64_t baseAddr = ent->baseAddr;
			uint64_t len = ent->len;

			if (place > baseAddr)
			{
				uint64_t delta = place-baseAddr;
				if (len < delta)
				{
					goto badSection;
				};

				baseAddr += delta;
				len -= delta;
			};

			kprintf("0x%016lx   0x%016lx   0x%lx\n", (uint64_t) vaddr, baseAddr, len);

			uint64_t phaddr;
			for (phaddr=baseAddr; phaddr<baseAddr+len; phaddr+=PAGE_SIZE)
			{
				*pte++ = phaddr | PT_PRESENT | PT_WRITE | PT_NOEXEC;
				vaddr += PAGE_SIZE;
			};

			badSection:;
		};

		mmap += ent->size + 4;
	};

	// now set up the heap
	pagetabReload();
	uint64_t memSize = vaddr - __virtMapArea;
	kprintf("\nSuccessfully mapped %lu bytes (%lu MB) of available memory, setting up the allocator...\n",
		memSize, memSize/1024/1024);
	vaddr = __virtMapArea;

	KOM_Pool *unusedPool = &komPools[KOM_POOL_UNUSED];
	int i;
	for (i=KOM_NUM_BUCKETS-1; i>=0; i--)
	{
		uint64_t bucketSize = KOM_BUCKET_SIZE(i);
		if (memSize & bucketSize)
		{
			kprintf("Bucket %2d: %p\n", i, vaddr);

			KOM_Header *header = (KOM_Header*) vaddr;
			memset(header, 0, sizeof(KOM_Header));

			unusedPool->buckets[i] = header;
			vaddr += bucketSize;
		};
	};

	nextVirtualAddr = (char*) (((uint64_t) vaddr + 0xFFF) & ~0xFFFUL);
	kprintf("\nStarting address for virtual allocations: 0x%p\n", nextVirtualAddr);
};

/**
 * Get the alignment requirement for blocks in the specified bucket.
 */
static uint64_t komGetAlignmentForBucket(int bucketIndex)
{
	uint64_t size = KOM_BUCKET_SIZE(bucketIndex);
	if (size >= PAGE_SIZE) return PAGE_SIZE;
	else return size;
};

/**
 * Check if the specified bucket in the pool contains any blocks which can be merged
 * and moved to the next bucket.
 */
static void _komMergeBlocks(KOM_Pool *pool, int bucketIndex)
{
	if (bucketIndex == KOM_NUM_BUCKETS-1)
	{
		return;
	};

	uint64_t alignmentForNextBucket = komGetAlignmentForBucket(bucketIndex+1);
	uint64_t sizeForThisBucket = KOM_BUCKET_SIZE(bucketIndex);

	KOM_Header *obj;
	for (obj=pool->buckets[bucketIndex]; (obj!=NULL&&obj->next!=NULL); obj=obj->next)
	{
		uint64_t objAddr = (uint64_t) obj;
		if ((objAddr & (alignmentForNextBucket-1)) == 0)
		{
			// correctly aligned for next bucket, see if consecutive with next block
			KOM_Header *expectedNext = (KOM_Header*) (objAddr + sizeForThisBucket);
			if (obj->next == expectedNext)
			{
				// unlink this double-object from this bucket and push to next one up
				if (obj->prev == NULL)
				{
					pool->buckets[bucketIndex] = expectedNext->next;
				}
				else
				{
					obj->prev->next = expectedNext->next;
				};

				if (expectedNext->next != NULL)
				{
					expectedNext->next->prev = obj->prev;
				};

				_komReleaseIntoPool(pool, obj, bucketIndex+1);
				return;
			};
		};
	};
};

static void _komReleaseIntoPool(KOM_Pool *pool, KOM_Header *obj, int bucketIndex)
{
	if (pool->buckets[bucketIndex] == NULL)
	{
		// list of blocks, trivial
		obj->prev = obj->next = NULL;
		pool->buckets[bucketIndex] = obj;
	}
	else
	{
		// find the object which is 'directly after' this one
		KOM_Header *justAfter;
		for (justAfter=pool->buckets[bucketIndex]; (justAfter!=NULL&&justAfter<obj); justAfter=justAfter->next);

		if (justAfter == NULL)
		{
			// there is no block after us, so we are the last
			KOM_Header *lastObj;
			for (lastObj=pool->buckets[bucketIndex]; lastObj->next!=NULL; lastObj=lastObj->next);

			obj->prev = lastObj;
			obj->next = NULL;

			lastObj->next = obj;
		}
		else
		{
			// attach us before
			if (justAfter->prev == NULL)
			{
				obj->prev = NULL;
				obj->next = justAfter;
				justAfter->prev = obj;
				pool->buckets[bucketIndex] = obj;
			}
			else
			{
				obj->prev = justAfter->prev;
				obj->next = justAfter;

				justAfter->prev->next = obj;
				justAfter->prev = obj;
			};
		};

		_komMergeBlocks(pool, bucketIndex);
	};
};

static void* _komAllocBlockFromPool(KOM_Pool *pool, int bucketIndex)
{
	if (bucketIndex >= KOM_NUM_BUCKETS)
	{
		return NULL;
	};

	if (pool->buckets[bucketIndex] != NULL)
	{
		// TODO: scan the blocks to choose which one is "preferable"
		KOM_Header *header = (KOM_Header*) pool->buckets[bucketIndex];
		if (header->next != NULL)
		{
			header->next->prev = header->prev;
		};

		pool->buckets[bucketIndex] = header->next;
		return header;
	}
	else
	{
		char *result = (char*) _komAllocBlockFromPool(pool, bucketIndex+1);
		if (result == NULL)
		{
			return NULL;
		};

		char *otherHalf = result + KOM_BUCKET_SIZE(bucketIndex);
		_komReleaseIntoPool(pool, (KOM_Header*) otherHalf, bucketIndex);

		return result;
	};
};

void* komAllocBlock(int bucket, int allowedPools)
{
	SpinIrqState irqState = spinlockAcquire(&komLock);
	
	int poolIndex;
	for (poolIndex=0; poolIndex<KOM_NUM_POOLS; poolIndex++)
	{
		if (allowedPools & (1 << poolIndex))
		{
			void *candidate = _komAllocBlockFromPool(&komPools[poolIndex], bucket);
			if (candidate != NULL)
			{
				spinlockRelease(&komLock, irqState);
				return candidate;
			};
		};
	};

	spinlockRelease(&komLock, irqState);
	return NULL;
};

void komReleaseBlock(void *block, int bucket)
{
	SpinIrqState irqState = spinlockAcquire(&komLock);
	_komReleaseIntoPool(&komPools[KOM_POOL_UNUSED], (KOM_Header*) block, bucket);
	spinlockRelease(&komLock, irqState);
};

void* komAllocVirtual(size_t size)
{
	size = (size + 0xFFF) & ~0xFFFUL;
	
	SpinIrqState irqState = spinlockAcquire(&komLock);
	char *result = nextVirtualAddr;
	nextVirtualAddr += size;
	spinlockRelease(&komLock, irqState);

	return result;
};