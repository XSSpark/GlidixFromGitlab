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

#include <glidix/hw/pagetab.h>
#include <glidix/hw/kom.h>
#include <glidix/util/string.h>

void pagetabGetNodes(const void *ptr, PageNodeEntry* nodes[4])
{
	// use recursive mapping to find the nodes
	nodes[0] = ((PageNodeEntry*)(((((uint64_t)ptr) >> 36) & (~(0x7UL | 0xFFFF000000000000UL))) | 0xFFFFFFFFFFFFF000));
	nodes[1] = ((PageNodeEntry*)(((((uint64_t)ptr) >> 27) & (~(0x7UL | 0xFFFF000000000000UL))) | 0xFFFFFFFFFFE00000));
	nodes[2] = ((PageNodeEntry*)(((((uint64_t)ptr) >> 18) & (~(0x7UL | 0xFFFF000000000000UL))) | 0xFFFFFFFFC0000000));
	nodes[3] = ((PageNodeEntry*)(((((uint64_t)ptr) >> 9) & (~(0x7UL | 0xFFFF000000000000UL))) | 0xFFFFFF8000000000));
};

void* pagetabGetPageStart(void *ptr)
{
	uint64_t addr = (uint64_t) ptr;
	return (void*) (addr & ~0xFFFUL);
};

uint64_t pagetabGetPhys(const void *ptr)
{
	uint64_t pte = *((uint64_t*)(((((uint64_t)ptr) >> 9) & (~(0x7UL | 0xFFFF000000000000UL))) | 0xFFFFFF8000000000));
	return (pte & PT_PHYS_MASK) | ((uint64_t) ptr & (PAGE_SIZE-1));
};

errno_t pagetabMapKernel(void *ptr, uint64_t phaddr, size_t size, uint64_t flags)
{
	if ((uint64_t) ptr & 0xFFF || phaddr & 0xFFF)
	{
		return EINVAL;
	};

	char *scan;
	for (scan=(char*)ptr; scan<(char*)ptr+size; (scan+=PAGE_SIZE,phaddr+=PAGE_SIZE))
	{
		PageNodeEntry *nodes[4];
		pagetabGetNodes(scan, nodes);

		int i;
		for (i=0; i<3; i++)
		{
			if ((nodes[i]->value & PT_PRESENT) == 0)
			{
				void *newLayer = komAllocBlock(KOM_BUCKET_PAGE, KOM_POOLBIT_ALL);
				if (newLayer == NULL)
				{
					return ENOMEM;
				};

				memset(newLayer, 0, PAGE_SIZE);
				nodes[i]->value = pagetabGetPhys(newLayer) | PT_PRESENT | PT_WRITE;
				invlpg(nodes[i+1]);
			};

			__sync_synchronize();
		};

		PageNodeEntry *pte = nodes[3];
		pte->value = phaddr | PT_PRESENT | flags;
		invlpg(scan);

		__sync_synchronize();
	};

	return 0;
};

void* pagetabMapPhys(uint64_t phaddr, size_t size, uint64_t flags)
{
	uint64_t mapEnd = phaddr + size;
	uint64_t offset = phaddr & 0xFFF;
	phaddr &= ~0xFFFUL;
	size = mapEnd - phaddr;

	char *result = (char*) komAllocVirtual(size);
	if (pagetabMapKernel(result, phaddr, size, flags) != 0)
	{
		return NULL;
	};

	return result + offset;
};