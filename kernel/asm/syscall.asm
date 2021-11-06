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

extern _sysCallTable
extern _sysCallCount
extern _sysCallInvalid

global _syscall_entry

; Entry pointer for system calls - `syscall` jumps here
_syscall_entry:
	; save the userspace stack pointer in a temporary location
	mov [gs:0x18], rsp

	; load the kernel stack from the CPU struct
	mov rsp, [gs:0x10]

	; save the return context on the stack (this will be needed if we need to
	; dispatch signals)
	sub rsp, 512
	fxsave [rsp]
	push rcx					; userspace return RIP
	push r11					; userspace return RFLAGS
	push qword [gs:0x18]				; userspace stack pointer
	push r15					; nonvolatile reg
	push r14					; nonvolatile reg
	push r13					; nonvolatile reg
	push r12					; nonvolatile reg
	push rbp					; nonvolatile reg
	push rbx					; nonvolatile reg
	push rbx					; push again for alignment reasons

	; load kernel data segments
	mov bx, 0x10
	mov ds, bx
	mov es, bx

	; save the return context to the current thread
	mov rbx, [gs:0x08]				; get current thread from CPU struct
	mov [rbx], rsp					; store in the `syscallContext` field of the thread

	; at this point it is safe to enable interrupts
	sti

	; ensure that the system call number (in RAX) isn't outside of bounds
	mov rbx, _sysCallCount
	cmp rax, [rbx]
	jae .invalid

	; look up the syscall on the system call table
	mov rbx, _sysCallTable
	shl rax, 3
	add rax, rbx
	mov rax, [rax]

	; ensure that the pointer isn't NULL
	test rax, rax
	jz .invalid

	; put the fourth argument back into RCX and call
	mov rcx, r10
	call rax

	; disable interrupts before returning
	cli

	; restore the context. note that we only clobbered RBX so the other
	; registers can be popped without restroing
	pop rbx
	pop rbx
	pop rbp
	add rsp, 4*8
	pop rdx					; userspace stack pointer -> RDX
	pop r11					; userspace RFLAGS
	pop rcx					; userspace RIP
	fxrstor [rsp]

	; restore userspace data segments
	mov r8w, 0x10
	mov ds, r8w
	mov es, r8w

	; go back to the userspace stack
	mov rsp, rdx

	; we now need:
	; RCX = the userspace RIP that sysret will return to
	; R11 = the userspace RFLAGs that sysret will return to
	; RAX = the return value
	; all other volatile registers will be wiped with zeroes, to prevent
	; any data leaks from kernel
	xor rdx, rdx
	xor r8, r8
	xor r9, r9
	xor r10, r10

	; NASM doesn't correctly encode sysret, so we encode it manually
	; (REX.W + SYSRET = 64-bit SYSRET)
	db 0x48, 0x0F, 0x07

.invalid:
	; invalid system call, pass the context and call `_sysCallInvalid`.
	; the function is expected to never return
	mov rdi, rsp
	call _sysCallInvalid