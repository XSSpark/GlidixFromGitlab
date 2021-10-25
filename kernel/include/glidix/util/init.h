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

#ifndef __glidix_util_init_h
#define	__glidix_util_init_h

#include <glidix/util/common.h>

/**
 * Bootloader feature flags.
 */
#define	KB_FEATURE_BOOTID		(1 << 0)
#define	KB_FEATURE_VIDEO		(1 << 1)
#define	KB_FEATURE_RSDP			(1 << 2)

/**
 * Define a kernel init action; syntax:
 * 	KERNEL_INIT_ACTION(func, name, ...deps)
 * Where `func` is a function taking no arguments and returning `void`, which will
 * be called when this init action is ready;
 * `name` is a string naming this init action (defined a macro of the format `KIA_*` to
 * hold this name);
 * All further arguments are names of init action which must execute before this one.
 * 
 * This macro is meant to be used in the global scope of a C file. It generates a
 * `KernelInitAction` struct in the `.ka_list` section, and these are concatenated into
 * a terminated array by `kernel.ld`. See there for more details.
 */
#define	KERNEL_INIT_ACTION(func, ...)\
	const char *__kiaLinks##func[] = {__VA_ARGS__, NULL};\
	SECTION(".kia_list") KernelInitAction __kia##func = {.initFunc = func, .links = __kiaLinks##func, .started = 0, .complete = 0}

/**
 * An entry in the memory map passed by the bootloader.
 */
typedef struct
{
	uint32_t			size;
	uint64_t			baseAddr;
	uint64_t			len;
	uint32_t			type;
} PACKED MemoryMapEntry;

/**
 * Pixel format, passed by the bootloader.
 */
typedef struct
{
	int				bpp;
	uint32_t			redMask;
	uint32_t			greenMask;
	uint32_t			blueMask;
	uint32_t			alphaMask;
	unsigned int			pixelSpacing;
	unsigned int			scanlineSpacing;
} PixelFormat;

/**
 * Kernel boot information. This is data passed from the bootloader to
 * the kernel.
 */
typedef struct
{
	uint64_t			features;
	uint64_t			kernelMain;
	uint64_t			gdtPointerVirt;
	uint32_t			pml4Phys;
	uint32_t			mmapSize;
	MemoryMapEntry*			mmap;
	uint64_t			initrdSize;
	uint64_t			end;
	uint64_t			initrdSymtabOffset;
	uint64_t			initrdStrtabOffset;
	uint64_t			numSymbols;
	
	/* only when KB_FEATURE_BOOTID is set */
	uint8_t				bootID[16];
	
	/* only when KB_FEATURE_VIDEO is set */
	uint8_t*			framebuffer;
	uint8_t*			backbuffer;
	uint32_t			fbWidth;
	uint32_t			fbHeight;
	PixelFormat			fbFormat;

	/* only when KB_FEATURE_RSDP is set */
	uint32_t			padBeforeRSDP;
	uint64_t			rsdpPhys;
} KernelBootInfo;

/**
 * Entry in the kernel init action table.
 */
typedef struct
{
	/**
	 * The function which will be called to perform this init task.
	 */
	void (*initFunc)();

	/**
	 * Link name table. The first entry is the name of this init action.
	 * The following entries are names of init actions which this one depends
	 * on, followed by NULL to indicate the end.
	 */
	const char **links;

	/**
	 * Initialized to 0, set to 1 when we begin the init process.
	 */
	int started;

	/**
	 * Initialized to 0, set to 1 when init has ended.
	 */
	int complete;

	// make this a multiple of 16 bytes
	uint64_t ignore;
} KernelInitAction;

/**
 * Pointer to the kernel boot information structure.
 */
extern KernelBootInfo *bootInfo;

/**
 * The array of kernel init actions; this is defined in `kernel.ld`, see there and the
 * documentation of `KERNEL_INIT_ACTION` for an explanation of the magic.
 */
extern KernelInitAction kiaList[];

/**
 * Kernel entry point. This function is called from the bootloader, and it must
 * never return!
 */
void kmain(KernelBootInfo *bootInfo);

#endif