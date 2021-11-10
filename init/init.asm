; Currently this is just a demo application for testing,
; read init will be put in this directory later

section .text

global _start
_start:

	mov rax, 3				; sys_fork
	syscall

	test rax, rax
	jz _child

	mov rax, 17				; sys_kill
	mov rdi, 2
	mov rsi, 9
	syscall

	jmp $
	
_child:
	jmp $