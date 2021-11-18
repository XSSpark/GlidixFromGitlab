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

.globl sigaction
.type sigaction, @function
sigaction:
	mov $1, %rax
	syscall

	// if return value was zero, return now
	test %eax, %eax
	jz sigaction_ret

	// nonzero return value: negated error number.
	// store it in errno (fs+0x18)
	neg %eax
	mov %eax, %fs:(0x18)
	mov $-1, %eax

sigaction_ret:
	ret
.size sigaction, .-sigaction

.globl write
.type write, @function
write:
	mov $7, %rax
	syscall

	// if return value is non-negative, return it
	mov $0x8000000000000000, %rcx
	test %rcx, %rax
	jz write_ret

	// negative return value; set errno
	neg %rax
	mov %eax, %fs:(0x18)
	mov $-1, %rax

write_ret:
	ret
.size write, .-write

.globl dup3
.type dup3, @function
dup3:
	mov $18, %rax
	syscall

	// if return value is non-negative, return it
	mov $0x80000000, %ecx
	test %ecx, %eax
	jz dup3_ret

	// negative return value; set errno
	neg %eax
	mov %eax, %fs:(0x18)
	mov $-1, %eax

dup3_ret:
	ret
.size dup3, .-dup3

.globl openat
.type openat, @function
openat:
	mov $4, %rax
	mov %rcx, %r10
	syscall

	mov $0x80000000, %ecx
	test %ecx, %eax
	jz openat_ret

	// negative return value; set errno
	neg %eax
	mov %eax, %fs:(0x18)
	mov $-1, %eax

openat_ret:
	ret
.size openat, .-openat

.globl open
.type open, @function
open:
	// shift the arguments
	mov %rdx, %rcx
	mov %rsi, %rdx
	mov %rdi, %rsi
	
	// insert dirfd = AT_FDCWD
	mov $0xFFFF, %rdi

	// tail-call openat
	jmp openat
.size open, .-open

.globl close
.type close, @function
close:
	mov $5, %rax
	syscall

	test %eax, %eax
	jz close_ret

	neg %eax
	mov %eax, %fs:(0x18)
	mov $-1, %eax

close_ret:
	ret
.size close, .-close

.globl read
.type read, @function
read:
	mov $6, %rax
	syscall

	// if return value is non-negative, return it
	mov $0x8000000000000000, %rcx
	test %rcx, %rax
	jz read_ret

	// negative return value; set errno
	neg %rax
	mov %eax, %fs:(0x18)
	mov $-1, %rax

read_ret:
	ret
.size read, .-read