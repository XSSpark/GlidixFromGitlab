;	Glidix kernel
;
;	Copyright (c) 2021, Madd Games.
;	All rights reserved.
;	
;	Redistribution and use in source and binary forms, with or without
;	modification, are permitted provided that the following conditions are met:
;	
;	* Redistributions of source code must retain the above copyright notice, this
;		list of conditions and the following disclaimer.
;	
;	* Redistributions in binary form must reproduce the above copyright notice,
;		this list of conditions and the following disclaimer in the documentation
;		and/or other materials provided with the distribution.
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
bits 64

%include "regs.inc"

[global _intCounter]
_intCounter dq 0

[global irq_ditch]

[global loadIDT]
[extern idtPtr]
loadIDT:
	mov rax, qword idtPtr
	lidt [rax]
	ret

[extern isrHandler]

isrCommon:
	pushAll

	mov			ax, 0x10
	mov			ds, ax
	mov			es, ax

	mov			rdi, rsp		; pass a pointer to registers as argument to isrHandler
	mov			rbx, rsp		; save the RSP (RBX is preserved, remember).
	and			rsp, ~0xF		; align on 16-byte boundary.
	
	sub			rsp, 512
	fxsave			[rsp]			; save the FPURegs
	mov			rsi, rsp		; pass FPURegs as second argument

	cld						; clear direction flag as it's undefined
	call	 		isrHandler

	fxrstor			[rsp]			; restore the FPU regs
	mov			rsp, rbx		; restore the real stack

	popAll
	add			rsp, 16
	iretq

%macro ISR_NOERRCODE 1
	global isr%1
	isr%1:
		cli
		push qword 0 
		push qword %1
		jmp isrCommon
%endmacro

%macro ISR_ERRCODE 1
	global isr%1
	isr%1:
		cli
		push qword %1
		jmp isrCommon
%endmacro

%macro IRQ 2
	global irq%1
	irq%1:
		cli
		push qword 0
		push qword %2
		jmp isrCommon
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE	 8
ISR_NOERRCODE 9
ISR_ERRCODE	 10
ISR_ERRCODE	 11
ISR_ERRCODE	12
ISR_ERRCODE	13
ISR_ERRCODE	14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31
ISR_NOERRCODE 48
ISR_NOERRCODE 49
ISR_NOERRCODE 50
ISR_NOERRCODE 51
ISR_NOERRCODE 52
ISR_NOERRCODE 53
ISR_NOERRCODE 54
ISR_NOERRCODE 55
ISR_NOERRCODE 56
ISR_NOERRCODE 57
ISR_NOERRCODE 58
ISR_NOERRCODE 59
ISR_NOERRCODE 60
ISR_NOERRCODE 61
ISR_NOERRCODE 62
ISR_NOERRCODE 63
ISR_NOERRCODE 64
ISR_NOERRCODE 65
ISR_NOERRCODE 112		; 0x70 - I_IPI_HALT
ISR_NOERRCODE 113		; 0x71 - I_IPI_SCHED_HINT
ISR_NOERRCODE 114		; 0x72 - I_IPI_PAGETAB_INVL

IRQ	0,	32
IRQ	1,	33
IRQ	2,	34
IRQ	3,	35
IRQ	4,	36
IRQ	5,	37
IRQ	6,	38
IRQ	7,	39
IRQ	8,	40
IRQ	9,	41
IRQ	10,	42
IRQ	11,	43
IRQ	12,	44
IRQ	13,	45
IRQ	14,	46
IRQ	15,	47

irq_ditch:
	; this is set as the interrupt handler for IRQs that shall be ditched; i.e. PIC interrupts.
	; we're not going to bother to send an EOI to it because we don't want it to continue sending
	; interrupts anyway.
	iretq