; Currently this is just a demo application for testing,
; read init will be put in this directory later

section .text

global _start
_start:
	mov rax, 1				; sys_sigaction
	mov rdi, 18				; SIGCHLD
	mov rsi, sigactionSIGCHLD
	xor rdx, rdx
	syscall

	mov rax, 3				; sys_fork
	syscall

	test rax, rax
	jz _child

	jmp $
	
_child:
	xor rax, rax				; sys_exit
	mov rdi, 0x57
	syscall

_onSIGCHLD:
	xchg bx, bx
	ret

sigactionSIGCHLD:
	dq _onSIGCHLD
	dq 0
	dq 0

section .data
status:
	dd 0