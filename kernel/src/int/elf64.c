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

#include <glidix/int/elf64.h>
#include <glidix/util/string.h>
#include <glidix/util/log.h>
#include <glidix/util/panic.h>
#include <glidix/hw/fpu.h>

/**
 * Enter the userspace context. This is defined in elf64.asm. It will switch to userspace,
 * start executing at `entry`, with stack pointer at `rsp`, and will load `fpuRegs`.
 * 
 * This never returns, because we will end up in userspace.
 */
noreturn void _elfEnter(user_addr_t entry, user_addr_t rsp, FPURegs *fpuRegs);

int elfReadInfo(File *fp, ElfInfo *info)
{
	Elf64_Ehdr ehdr;
	Elf64_Phdr phdr[ELF_SEGMENT_MAX];

	memset(info, 0, sizeof(ElfInfo));

	// try reading the ELF header
	if (vfsPRead(fp, &ehdr, sizeof(Elf64_Ehdr), 0) != sizeof(Elf64_Ehdr))
	{
		return -ENOEXEC;
	};

	// validate the header
	if (memcmp(ehdr.e_ident, "\x7f" "ELF", 4) != 0)
	{
		return -ENOEXEC;
	};

	if (ehdr.e_ident[EI_CLASS] != ELFCLASS64)
	{
		return -ENOEXEC;
	};

	if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
	{
		return -ENOEXEC;
	};

	if (ehdr.e_ident[EI_VERSION] != 1)
	{
		return -ENOEXEC;
	};

	if (ehdr.e_type != ET_EXEC)
	{
		return -ENOEXEC;
	};

	if (ehdr.e_machine != EM_X86_64)
	{
		return -ENOEXEC;
	};
	
	if (ehdr.e_phentsize != sizeof(Elf64_Phdr))
	{
		return -ENOEXEC;
	};

	if (ehdr.e_phnum > ELF_SEGMENT_MAX)
	{
		return -ENOEXEC;
	};

	// load the entry point
	info->entry = ehdr.e_entry;

	// try to load the program headers
	size_t phdrTotalSize = sizeof(Elf64_Phdr) * ehdr.e_phnum;
	if (vfsPRead(fp, &phdr, phdrTotalSize, ehdr.e_phoff) != phdrTotalSize)
	{
		return -ENOEXEC;
	};

	// first check if there is a PT_INTERP program header
	int i;
	for (i=0; i<ehdr.e_phnum; i++)
	{
		Elf64_Phdr *proghead = &phdr[i];

		if (proghead->p_type == PT_INTERP)
		{
			if (proghead->p_filesz > ELF_INTERP_MAX-1)
			{
				return -ENOEXEC;
			};

			if (vfsPRead(fp, info->interp, proghead->p_filesz, proghead->p_offset) != proghead->p_filesz)
			{
				return -ENOEXEC;
			};

			return 0;
		};
	};

	// no PT_INTERP section, so only PT_LOAD and PT_NULL allowed
	ElfSegment *seg = info->segs;
	for (i=0; i<ehdr.e_phnum; i++)
	{
		Elf64_Phdr *proghead = &phdr[i];

		if (proghead->p_type == PT_NULL)
		{
			// skip
		}
		else if (proghead->p_type == PT_LOAD)
		{
			if ((proghead->p_vaddr & 0xFFF) != (proghead->p_offset & 0xFFF))
			{
				// not congruent modulo the page size
				return -ENOEXEC;
			};

			if (proghead->p_filesz > proghead->p_memsz)
			{
				// file size is larger than memory size
				return -ENOEXEC;
			};

			seg->vaddr = proghead->p_vaddr & ~0xFFF;
			seg->offset = proghead->p_offset & ~0xFFF;
			seg->memsz = proghead->p_memsz + (proghead->p_vaddr & 0xFFF);
			seg->filesz = proghead->p_filesz + (proghead->p_vaddr & 0xFFF);

			if (proghead->p_flags & PF_R) seg->prot |= PROT_READ;
			if (proghead->p_flags & PF_W) seg->prot |= PROT_WRITE;
			if (proghead->p_flags & PF_X) seg->prot |= PROT_EXEC;

			seg++;
			info->numSegments++;
		}
		else
		{
			// unexpected phdr type
			return -ENOEXEC;
		};
	};

	return 0;
};

static int elfExecStatic(File *fp, ElfInfo *info, const char **argv, const char **envp, int execfd)
{
	errno_t err;

	// perform pre-exec cleanup (unmapping all memory, etc)
	procBeginExec();

	// map segments into memory
	int i;
	for (i=0; i<info->numSegments; i++)
	{
		ElfSegment *seg = &info->segs[i];

		// start by mapping the whole memory-size region as anonymous
		if (procMap(seg->vaddr, seg->memsz, seg->prot, MAP_ANON | MAP_FIXED | MAP_PRIVATE, NULL, 0, &err) != seg->vaddr)
		{
			panic("TODO: can't handle failure yet (mapping the memory part)");
		};

		// now map the file part of it
		if (seg->filesz != 0)
		{
			if (procMap(seg->vaddr, seg->filesz, seg->prot, MAP_FIXED | MAP_PRIVATE, fp, seg->offset, &err) != seg->vaddr)
			{
				panic("TODO: can't handle failure yet (mapping the file part)");
			};
		};

		// TODO: we will need to zero out the end of a page if the file and memory size don't match, and the file
		// ends at somewhere other than a page boundary
		if (seg->filesz != seg->memsz)
		{
			panic("can't handle BSS yet");
		};
	};

	// map the stack
	if (procMap(ELF_USER_STACK_BASE, ELF_USER_STACK_SIZE,
		PROT_READ | PROT_WRITE, MAP_ANON | MAP_FIXED | MAP_PRIVATE, NULL, 0, &err) != ELF_USER_STACK_BASE)
	{
		panic("TODO: can't handle failure yet (the stack)");
	};

	// initialize the stack
	user_addr_t rsp = ELF_USER_STACK_BASE + ELF_USER_STACK_SIZE;

	// TODO: push all the args and stuff

	// set up FPU regs
	FPURegs fpuRegs;
	memset(&fpuRegs, 0, sizeof(FPURegs));
	fpuRegs.mxcsr = MX_PM | MX_UM | MX_OM | MX_ZM | MX_DM | MX_IM;

	// success
	vfsClose(fp);
	_elfEnter(info->entry, rsp, &fpuRegs);
};

int elfExec(File *fp, const char *path, const char **argv, const char **envp)
{
	ElfInfo info;

	int status = elfReadInfo(fp, &info);
	if (status != 0)
	{
		return status;
	};

	if (info.interp[0] != 0)
	{
		// TODO: support the ELF interpreter
		return -ENOEXEC;
	}
	else
	{
		// static executable
		return elfExecStatic(fp, &info, argv, envp, -1);
	};
};