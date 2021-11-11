.text

// generic system call function (from <sys/call.h>)
.globl __syscall
.type __syscall, @function
__syscall:
	mov %rdi, %rax
	mov %rsi, %rdi
	mov %rdx, %rsi
	mov %rcx, %rdx
	mov %r8, %r10
	mov %r9, %r8
	mov 8(%rsp), %r9
	syscall
	ret
.size __syscall, .-__syscall

.globl _exit
.type _exit, @function
_exit:
	xor %rax, %rax
	syscall
	// no return
.size _exit, .-_exit

.globl pthread_self
.type pthread_self, @function
pthread_self:
	mov $19, %rax
	syscall
	ret
.size pthread_self, .-pthread_self

.globl raise
.type raise, @function
raise:
	mov $20, %rax
	syscall

	// if return value was zero, return now
	test %eax, %eax
	jz raise_ret

	// nonzero return value: negated error number.
	// store it in errno (fs+0x18)
	neg %eax
	mov %eax, %fs:(0x18)
	mov $-1, %eax

raise_ret:
	ret
.size raise, .-raise

.globl mmap
.type mmap, @function
mmap:
	mov $21, %rax
	mov %rcx, %r10
	syscall

	// if the sign bit is clear, just return
	mov $1, %rcx
	shl $63, %rcx
	test %rcx, %rax
	jz mmap_ret

	// error occured, set errno and return MAP_FAILED
	neg %rax
	mov %eax, %fs:(0x18)
	xor %rax, %rax
	neg %rax

mmap_ret:
	ret
.size mmap, .-mmap