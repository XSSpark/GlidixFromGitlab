; Currently this is just a demo application for testing,
; read init will be put in this directory later

section .text

global _start
_start:
	mov rax, 4
	mov rdi, 0xFFFF				; AT_FDCWD
	mov rsi, filename
	mov rdx, 3				; O_RDWR
	syscall
	xchg bx, bx

	jmp $

filename:
	db '/initrd-console', 0