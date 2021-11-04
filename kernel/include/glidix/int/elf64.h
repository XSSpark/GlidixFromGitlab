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

#ifndef __glidix_int_elf64_h
#define	__glidix_int_elf64_h

#include <glidix/util/common.h>
#include <glidix/thread/process.h>
#include <glidix/fs/file.h>

/**
 * The base address of the initial userspace stack.
 */
#define	ELF_USER_STACK_BASE		0x200000

/**
 * The size of the initial userspace stack.
 */
#define	ELF_USER_STACK_SIZE		0x200000

/**
 * Minimum address for ELF segments.
 */
#define	ELF_VADDR_MIN			0x400000

/**
 * Maximum allowed number of segments in an ELF file.
 */
#define	ELF_SEGMENT_MAX			32

/**
 * Maximum allowed length of the interpreter path in an ELF file.
 */
#define	ELF_INTERP_MAX			256

typedef	uint64_t			Elf64_Addr;
typedef	uint16_t			Elf64_Half;
typedef	uint64_t			Elf64_Off;
typedef	int32_t				Elf64_Sword;
typedef	int64_t				Elf64_Sxword;
typedef	uint32_t			Elf64_Word;
typedef	uint64_t			Elf64_Xword;
typedef	int64_t				Elf64_Sxword;

#define	EI_MAG0				0
#define	EI_MAG1				1
#define	EI_MAG2				2
#define	EI_MAG3				3
#define	EI_CLASS			4
#define	EI_DATA				5
#define	EI_VERSION			6
#define	EI_OSABI			7
#define	EI_ABIVERSION			8
#define	EI_PAD				9
#define	EI_NIDENT			16

#define	ELFCLASS32			1
#define	ELFCLASS64			2

#define	ELFDATA2LSB			1
#define	ELFDATA2MSB			2

#define	ET_NONE				0
#define	ET_REL				1		/* for modules */
#define	ET_EXEC				2
#define	ET_DYN				3

#define	EM_X86_64			62		/* e_machine for x86_64 */

#define	PT_NULL				0
#define	PT_LOAD				1
#define	PT_DYNAMIC			2
#define	PT_INTERP			3
#define	PT_NOTE				4
#define	PT_SHLIB			5
#define	PT_PHDR				6

#define	PF_X				0x1
#define	PF_W				0x2
#define	PF_R				0x4

#define	SHT_NULL			0
#define	SHT_PROGBITS			1
#define	SHT_SYMTAB			2
#define	SHT_STRTAB			3
#define	SHT_RELA			4
#define	SHT_HASH			5
#define	SHT_DYNAMIC			6
#define	SHT_NOTE			7
#define	SHT_NOBITS			8
#define	SHT_REL				9
#define	SHT_SHLIB			10
#define	SHT_DYNSYM			11

#define ELF64_R_SYM(i)			((i) >> 32)
#define ELF64_R_TYPE(i)			((i) & 0xffffffffL)
#define ELF64_R_INFO(s, t)		(((s) << 32) + ((t) & 0xffffffffL))

#define	R_X86_64_NONE			0
#define	R_X86_64_64			1
#define	R_X86_64_GLOB_DAT		6
#define	R_X86_64_JUMP_SLOT		7
#define	R_X86_64_RELATIVE		8

#define	AT_NULL				0
#define	AT_IGNORE			1
#define	AT_EXECFD			2

typedef struct
{
	unsigned char			e_ident[EI_NIDENT];
	Elf64_Half			e_type;
	Elf64_Half			e_machine;
	Elf64_Word			e_version;
	Elf64_Addr			e_entry;
	Elf64_Off			e_phoff;
	Elf64_Off			e_shoff;
	Elf64_Word			e_flags;
	Elf64_Half			e_ehsize;
	Elf64_Half			e_phentsize;
	Elf64_Half			e_phnum;
	Elf64_Half			e_shentsize;
	Elf64_Half			e_shnum;
	Elf64_Half			e_shstrndx;
} Elf64_Ehdr;

typedef struct
{
	Elf64_Word			p_type;
	Elf64_Word			p_flags;
	Elf64_Off			p_offset;
	Elf64_Addr			p_vaddr;
	Elf64_Addr			p_paddr;
	Elf64_Xword			p_filesz;
	Elf64_Xword			p_memsz;
	Elf64_Xword			p_align;
} Elf64_Phdr;

typedef struct
{
	Elf64_Word			sh_name;
	Elf64_Word			sh_type;
	Elf64_Xword			sh_flags;
	Elf64_Addr			sh_addr;
	Elf64_Off			sh_offset;
	Elf64_Xword			sh_size;
	Elf64_Word			sh_link;
	Elf64_Word			sh_info;
	Elf64_Xword			sh_addralign;
	Elf64_Xword			sh_entsize;
} Elf64_Shdr;

typedef struct
{
	Elf64_Addr			r_offset;
	Elf64_Xword			r_info;
	Elf64_Sxword			r_addend;
} Elf64_Rela;

typedef struct
{
	Elf64_Word			st_name;
	unsigned char			st_info;
	unsigned char			st_other;
	Elf64_Half			st_shndx;
	Elf64_Addr			st_value;
	Elf64_Xword			st_size;
} Elf64_Sym;

typedef struct
{
	Elf64_Sxword			d_tag;
	union
	{
		Elf64_Xword		d_val;
		Elf64_Addr		d_ptr;
	} d_un;
} Elf64_Dyn;

typedef struct
{
	uint32_t			a_type;
	union
	{
		uint64_t		a_val;
		void*			a_ptr;
	} a_un;
} Elf64_Auxv;

/**
 * Represents a segment derived from a program header in an ELF64 file.
 */
typedef struct
{
	/**
	 * Virtual base address.
	 */
	user_addr_t vaddr;

	/**
	 * Offset within the file.
	 */
	off_t offset;

	/**
	 * Memory size.
	 */
	size_t memsz;

	/**
	 * File size.
	 */
	size_t filesz;

	/**
	 * Protection flags.
	 */
	int prot;
} ElfSegment;

/**
 * Represents information loaded from an ELF64 file.
 */
typedef struct
{
	/**
	 * Segments loaded from the file.
	 */
	ElfSegment segs[ELF_SEGMENT_MAX];

	/**
	 * Number of segments.
	 */
	int numSegments;

	/**
	 * Interpreter path (or empty string if no interpreter requested).
	 */
	char interp[ELF_INTERP_MAX];

	/**
	 * The entry point.
	 */
	user_addr_t entry;
} ElfInfo;

/**
 * Try to load ELF information from the specified file. On success, returns 0 and fills out `info`.
 * On error, returns a negated error number.
 */
int elfReadInfo(File *fp, ElfInfo *info);

/**
 * Execute an ELF file. This performs the exec for the ELF executable format, see `exec.h` for more
 * info.
 */
int elfExec(File *fp, const char *path, const char **argv, const char **envp);

#endif