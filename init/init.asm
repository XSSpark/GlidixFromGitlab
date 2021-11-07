; Currently this is just a demo application for testing,
; read init will be put in this directory later

section .text

global _start
_start:
	mov rax, 1				; sigaction
	mov rdi, 11				; SIGSEGV
	mov rsi, sigactionSIGSEGV		; the struct sigaction for our handler
	xor rdx, rdx				; oldact = NULL
	syscall

	xor rax, rax
	mov [rax], rax

onSIGSEGV:
	ret

sigactionSIGSEGV:
	dq onSIGSEGV
	dq 0
	dq 0