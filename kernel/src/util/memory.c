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

#include <glidix/util/memory.h>
#include <glidix/hw/kom.h>
#include <glidix/util/string.h>

void* kmalloc(size_t size)
{
	if (size == 0)
	{
		return NULL;
	};

	size_t totalSize = size + sizeof(HeapHeader);
	
	int i;
	for (i=0; i<KOM_NUM_BUCKETS; i++)
	{
		if (KOM_BUCKET_SIZE(i) >= totalSize) break;
	};

	if (i == KOM_NUM_BUCKETS)
	{
		return NULL;
	};

	HeapHeader *header = (HeapHeader*) komAllocBlock(i, KOM_POOLBIT_ALL);
	if (header == NULL)
	{
		return NULL;
	};

	header->actualSize = size;
	header->bucket = i;

	return &header[1];
};

void* krealloc(void *ptr, size_t newSize)
{
	if (newSize == 0)
	{
		return NULL;
	};

	if (ptr == NULL)
	{
		return kmalloc(newSize);
	};

	size_t totalNewSize = sizeof(HeapHeader) + newSize;

	HeapHeader *header = (HeapHeader*) ptr - 1;
	if (newSize < header->actualSize)
	{
		// block is shrinking
		while (1)
		{
			if (header->bucket == 0)
			{
				// there are no smaller buckets
				return ptr;
			};

			uint64_t prevBucketSize = KOM_BUCKET_SIZE(header->bucket-1);
			if (prevBucketSize >= totalNewSize)
			{
				// the previous bucket can fit us, move down
				char *otherHalf = (char*) header + prevBucketSize;
				komReleaseBlock(otherHalf, header->bucket-1);
				header->bucket--;
			}
			else
			{
				// the smaller bucket wouldn't fit us, work done
				return ptr;
			};
		};
	};

	if (KOM_BUCKET_SIZE(header->bucket) >= totalNewSize)
	{
		// the new size fits in the current bucket
		return ptr;
	};

	// worst case: have to do a full reallocation
	void *result = kmalloc(newSize);
	if (result == NULL)
	{
		return NULL;
	};

	memcpy(result, ptr, header->actualSize);
	kfree(ptr);

	return result;
};

void kfree(void *ptr)
{
	if (ptr != NULL)
	{
		HeapHeader *header = (HeapHeader*) ptr - 1;
		komReleaseBlock(header, header->bucket);
	};
};