; Currently this is just a demo application for testing,
; read init will be put in this directory later

section .text

global _start
_start:
	mov rax, 999
	syscall