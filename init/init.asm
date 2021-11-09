; Currently this is just a demo application for testing,
; read init will be put in this directory later

section .text

global _start
_start:
	mov rax, 3				; sys_fork
	syscall

	test rax, rax
	jz _child

	mov rdi, rax				; PID into RDI
	mov rax, 12				; sys_waitpid
	mov rsi, status				; &status
	xor rdx, rdx				; flags = 0
	syscall

	mov ecx, [status]
	xchg bx, bx
	jmp $

_child:
	xor rax, rax				; sys_exit
	mov rdi, 0x23
	syscall

section .data
status:
	dd 0