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

global _schedYield
_schedYield:
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