; Currently this is just a demo application for testing,
; real init will be put in this directory later

section .text

extern main

global _start
global openat
global write
global dup3

_start:
	call main
	mov rdi, rax
	xor rax, rax
	syscall

openat:
	mov rax, 4
	mov r10, rcx
	syscall
	ret

write:
	mov rax, 7
	syscall
	ret

dup3:
	mov rax, 18
	syscall
	ret