;	Glidix kernel
;
;	Copyright (c) 2021, Madd Games.
;	All rights reserved.
;	
;	Redistribution and use in source and binary forms, with or without
;	modification, are permitted provided that the following conditions are met:
;	
;	* Redistributions of source code must retain the above copyright notice, this
;	  list of conditions and the following disclaimer.
;	
;	* Redistributions in binary form must reproduce the above copyright notice,
;	  this list of conditions and the following disclaimer in the documentation
;	  and/or other materials provided with the distribution.
;	
;	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
;	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
;	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
;	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
;	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
;	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
;	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

section .text

extern _cpuApEntry
global _cpuTrampolineStart
global _cpuTrampolineEnd

bits 16
_cpuTrampolineStart:
	jmp 0:0xA005
	cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax

	; tell the BSP that we've started
	mov ax, 1
	mov [0xB000], ax			; flagAP2BSP

	; loop until the BSP enables its flag
.waitForBSP:
	mov ax, [0xB004]
	test ax, ax
	jz .waitForBSP

	; enable paging and PAE
	mov eax, 10100000b
	mov cr4, eax

	; set the PML4 to the temporary one
	mov eax, 0xC000
	mov cr3, eax

	; enable long mode and NX in the EFER MSR
	mov ecx, 0xC0000080
	rdmsr
	or eax, (1 << 8)
	or eax, (1 << 11)
	wrmsr

	; load the temporary GDT
	lgdt [0xB018]

	; enable paging, write-protect and protected mode at once
	mov eax, cr0
	or eax, (1 << 31)
	or eax, (1 << 16)
	or eax, (1 << 0)
	mov cr0, eax

	; jump to 64-bit mode
	jmp 0x08:(trampoline64 - _cpuTrampolineStart + 0xA000)

bits 64
trampoline64:
	; load 64-bit segments
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	xor ax, ax
	mov ss, ax

	; load the real GDTPointer and set the GDT
	mov rax, [0xB010]
	lgdt [rax]

	; load the real PML4
	mov rax, [0xB028]
	mov cr3, rax

	; can't remember why we apparently need this
	mov rax, cr0
	mov cr0, rax

	; load the IDT
	mov rax, [0xB030]
	lidt [rax]

	; load the requested stack pointer and align it properly
	mov rax, [0xB038]
	and rax, ~0xF
	mov rsp, rax
	
	; call _cpuApEntry() which must never return!
	; we need to load its address into RAX first, otherwise it'll be relative,
	; so we'll be using our 'lowmem' address instead of the real virtual address
	mov rax, _cpuApEntry
	call rax

_cpuTrampolineEnd: