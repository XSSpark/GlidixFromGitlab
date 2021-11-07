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

section .user_aux
bits 64

global userAuxSigReturn

userAuxSigReturn:
	; restore the old signal mask
	mov rax, 2				; sys_sigmask
	mov rdi, 2				; SIG_SETMASK
	mov rsi, [rsp+8]			; get sigmask from the context
	syscall

	; load the FPU regs from the context
	fxrstor [rsp+0x30]

	; set the stack pointer to the kmcontext_gpr_t pointer.
	; this pops context header (uc_link, etc), the FPU regs,
	; and the siginfo_t, and leaves us at the kmcontext_gpr_t
	; pointer, which is just above the red zone
	mov rsp, [rsp+0x230]

	; pop off the copy of return RSP and ignore it
	pop rax

	; now pop off the values of the registers
	pop rax
	pop rbx
	pop rcx
	pop rdx
	pop rsi
	pop rdi
	pop rbp
	pop r8
	pop r9
	pop r10
	pop r11
	pop r12
	pop r13
	pop r14
	pop r15

	; 2 things left on the stack: the return RIP, and the red
	; zone; use RET to jump to the return address and to pop off
	; the 128 bytes
	ret 128