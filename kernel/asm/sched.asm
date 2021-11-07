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
bits 64

extern _schedNext
extern schedExitThread
extern schedSuspend
extern cpuGetTSS
extern cpuGetGDT

global _schedYield
_schedYield:
	; preserve FPU regs (this includes control regs and whatnot)
	sub rsp, 512+8
	fxsave [rsp]
	push rax

	; push the registers which must be preserved
	push rbx
	push rbp
	push r12
	push r13
	push r14
	push r15
	push rdi		; the IRQ state

	; tell the scheduler where this return stack is, and switch
	; to the next task.
	mov rdi, rsp
	call _schedNext

global _schedIdle
_schedIdle:
	; move to the idle stack passed as an argument
	mov rsp, rdi

	; enable interrupts, then keep halting; when an interrupt
	; arrives, we will switch to a different context.
	sti
.loop:
	hlt
	call schedSuspend
	jmp .loop

global _schedReturn
_schedReturn:
	; move to the return stack
	mov rsp, rdi

	; pop the registers in the reverse order from _schedYield
	pop rax
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	pop rbx

	pop rcx
	fxrstor [rsp]
	add rsp, 512+8

	; restore the IRQ state and return
	pushf
	or [rsp], rax
	popf
	ret

global _schedThreadEntry
_schedThreadEntry:
	; we are passed:
	; R15 = entry func
	; R14 = entry func argument
	mov rdi, r14
	call r15
	call schedExitThread

global _schedUpdateTSS
_schedUpdateTSS:
	push rbx
	mov rbx, rdi

	call cpuGetTSS
	mov [rax+4], rbx			; put the kernel stack in the TSS
	
	call cpuGetGDT
	mov [rax+0x35], byte 11101001b		; reload the access field

	mov rax, 0x33
	ltr ax

	pop rbx
	ret

global _schedEnterSignalHandler
_schedEnterSignalHandler:
	; RDI = signal number (will be passed to handler)
	; RSI = siginfo_t userspace pointer (will be passed to handler)
	; RDX = context pointer (will be passed to handler, also used to find stack)
	; RCX = the handler address (in userspace)

	; figure out the userspace stack pointer and store in RAX (this is just 8
	; below the context pointer)
	lea rax, [rdx-8]

	; set up a stack for IRETQ
	push 0x23			; userspace SS
	push rax			; userspace stack pointer
	pushf				; userspace flags
	or qword [rsp], (1 << 9)	; ensure interrupts will be enabled
	push 0x1B			; userspace CS
	push rcx			; userspace IP

	; set up userspace data segments
	cli
	mov ax, 0x23
	mov ds, ax
	mov es, ax

	; zero out the GPRs except for the arguments to the handler
	xor rax, rax
	xor rbx, rbx
	xor rcx, rcx
	xor rbp, rbp
	xor r8, r8
	xor r9, r9
	xor r10, r10
	xor r11, r11
	xor r12, r12
	xor r13, r13
	xor r14, r14
	xor r15, r15

	; go to the handler
	iretq