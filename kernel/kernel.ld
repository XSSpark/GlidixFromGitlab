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

/**
 * Virtual base address of the kernel; this is chosen such that the kernel image
 * occupied pml4[510] only.
 */
KERNEL_VMA = 0xFFFFFF0000000000;

/**
 * Physical base address of the APIC register space.
 */
APIC_PHYS_BASE = 0xFEE00000;

PHDRS
{
	text PT_LOAD;
	data PT_LOAD;
	initrd 0x60000001;			/* tells bootloader to load the initrd here */
	apic 0x60000002;			/* tells the bootloader to map to APIC register space but not load; disable cache */
}

SECTIONS
{
	
	. = KERNEL_VMA;
	
	.text :
	{
		code = .;
		*(.text)
		*(.rodata)
		. = ALIGN(4096);
		*(.usup_text)
	} :text
	
	. = ALIGN(4096);
	
	.data :
	{
		data = .;
		*(.data)
	} :data
	
	.bss :
	{
		bss = .;
		*(.bss)
	} :data
	
	. = ALIGN(4096);
	
	.stack (NOLOAD) :
	{
		*(.stack)
	} :data
	_stackBottom = .;
	
	. = ALIGN(4096);
	
	.initrd (NOLOAD) :
	{
		*(.initrd)
	} :initrd

	. = ALIGN(4096);
	.apic (NOLOAD) : AT(APIC_PHYS_BASE)
	{
		apic = .;
		. += 0x1000;
	} :apic

	. = ALIGN(4096);
	__virtMapArea = .;
}