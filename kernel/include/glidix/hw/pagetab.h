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

#ifndef __glidix_hw_pagetab_h
#define	__glidix_hw_pagetab_h

#include <glidix/util/common.h>
#include <glidix/util/errno.h>
#include <glidix/hw/cpu.h>

// TODO: when mapping, lock pages!

/**
 * Page entry flags.
 */
#define	PT_PRESENT			(1UL << 0)
#define	PT_WRITE			(1UL << 1)
#define	PT_USER				(1UL << 2)
#define	PT_NOCACHE			(1UL << 4)
#define	PT_PROT_READ			(1UL << 59)
#define	PT_PROT_WRITE			(1UL << 60)
#define	PT_PROT_EXEC			(1UL << 61)
#define	PT_COW				(1UL << 62)
#define	PT_NOEXEC			(1UL << 63)

/**
 * Page table physical address mask.
 */
#define	PT_PHYS_MASK			0x0000FFFFFFFFF000UL

/**
 * Mask for the glidix permission bits.
 */
#define	PT_PROT_MASK			(PT_PROT_READ | PT_PROT_WRITE | PT_PROT_EXEC)

/**
 * Size of a page.
 */
#define	PAGE_SIZE			0x1000

/**
 * Page fault flags (as provided by the CPU in the `errCode`).
 */
#define	PF_PRESENT			(1 << 0)
#define	PF_WRITE			(1 << 1)
#define	PF_USER				(1 << 2)
#define	PF_RESERVED			(1 << 3)
#define	PF_FETCH			(1 << 4)

/**
 * Format of a page table entry at each level (PML4, PDPT, etc).
 * Simply wraps a `uint64_t`.
 */
typedef struct
{
	uint64_t value;
} PageNodeEntry;

/**
 * Invalidate the TLB containing `ptr`. This is needed after you've updated page
 * tables for that pointer, so that they are reloaded.
 */
static inline void invlpg(const void* ptr)
{
	ASM ( "invlpg (%0)" : : "b" (ptr) : "memory" );
};

/**
 * Get the four nodes of the page table for `ptr`, in the order from highest to
 * lowest. That is, `nodes[0]` will be the PML4 entry, `nodes[1]` will be the
 * PDPT entry, etc.
 * 
 * Note that if, for example, `nodes[0]` is not present, then `nodes[1]` will
 * be an invalid, unmapped pointer! Map a higher node to physical memory to
 * ensure that the following node is also mapped.
 */
void pagetabGetNodes(const void *ptr, PageNodeEntry* nodes[4]);

/**
 * Get the pointer to the beginning of the page containing `ptr`.
 */
void* pagetabGetPageStart(void *ptr);

/**
 * Reload the page table. This can be called if major changes were made and the
 * whole page table needs flushing.
 */
void pagetabReload();

/**
 * Get the physical address for the specified virtual address. This assumed that
 * the specified address is mapped!
 */
uint64_t pagetabGetPhys(const void *ptr);

/**
 * Map the virtual memory starting at `addr` to physical memory starting at `physBase`,
 * with the specified `size`. Returns 0 on success or an error number on error.
 * 
 * `flags` is a bitwise-OR of one or more `PT_*` flags. `PT_PRESENT` will be set
 * unconditionally and so is optional.
 * 
 * `ENOMEM` is returned if page table allocations failed.
 * 
 * `EINVAL` is returend if either `ptr` or `physBase` is not page-aligned.
 */
errno_t pagetabMapKernel(void *ptr, uint64_t physBase, size_t size, uint64_t flags);

/**
 * Map an arbitrary physical pointer into virtual memory. Note that it is impossible
 * to free the address space allocated by this function. Returns NULL if the allocation
 * was not possible (out of memory for page tables).
 * 
 * `flags` are the flags to be passed to `pagetabMapKernel()`.
 * 
 * The physical memory address does not need to be aligned in any way.
 */
void* pagetabMapPhys(uint64_t phaddr, size_t size, uint64_t flags);

/**
 * Get the current physical address of the PML4.
 */
static inline uint64_t pagetabGetCR3()
{
	uint64_t cr3;
	ASM ("mov %%cr3, %0" : "=a" (cr3));
	return cr3;
};

/**
 * Switch to the specified physical address of a PML4.
 */
static inline void pagetabSetCR3(uint64_t cr3)
{
	cpuGetCurrent()->currentCR3 = cr3;
	ASM ("mov %0, %%cr3" : : "a" (cr3));
};

/**
 * Mark the userspace auxiliary code as accessible from user mode.
 */
void pagetabSetupUserAux();

#endif