; Currently this is just a demo application for testing,
; read init will be put in this directory later

section .text

global _start
_start:
	mov rax, 4				; sys_open
	mov rdi, 0xFFFF				; AT_FDCWD
	mov rsi, filename
	mov rdx, 3				; O_RDWR
	syscall

	mov rdi, rax				; fd into RDI
	mov rax, 7				; sys_write
	mov rsi, hello
	mov rdx, 5
	syscall

	jmp $

filename:
	db '/initrd-console', 0

hello:
	db 'hello'