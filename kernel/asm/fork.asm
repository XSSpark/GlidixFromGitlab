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

global _forkEnterChild
_forkEnterChild:
	; load GPRs from the context we were passed in
	mov rbx, [rdi+0x08]
	mov rbp, [rdi+0x10]
	mov r12, [rdi+0x18]
	mov r13, [rdi+0x20]
	mov r14, [rdi+0x28]
	mov r15, [rdi+0x30]
	mov rsi, [rdi+0x38]		; RSI = userspace stack
	mov r11, [rdi+0x40]		; R11 = userspace RFLAGS
	mov rcx, [rdi+0x48]		; RCX = userspace RIP
	fxrstor [rdi+0x50]

	; disable interrupts and set userspace segments
	cli
	mov dx, 0x23
	mov ds, dx
	mov es, dx

	; go to the userspace stack
	mov rsp, rsi

	; clear all the volatile regs (except R11 and RCX which we need to do the return);
	; most notably, clearing `rax` makes it look like `fork()` returns 0 to the child
	xor rax, rax
	xor rdx, rdx
	xor rsi, rsi
	xor rdi, rdi
	xor r8, r8
	xor r9, r9
	xor r10, r10

	; return using SYSRET (NASM doesn't encode 64-bit SYSRET correctly so we enter it
	; manually)
	db 0x48, 0x0F, 0x07