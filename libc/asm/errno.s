.text
.globl __errnoptr

.type __errnoptr,@function
__errnoptr:
	// errno is at offset 0x18 in the thread block
	mov %fs:(0), %rax
	add $0x18, %rax
	ret
.size __errnoptr, .-__errnoptr
