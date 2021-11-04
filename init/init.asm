; Currently this is just a demo application for testing,
; read init will be put in this directory later

section .text

global _start
_start:
	mov eax, 0xBEEFBEEF
	xchg bx, bx
	jmp _start