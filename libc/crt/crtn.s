.section .init
	pop %rbp
	ret

.section .fini
	pop %rbp
	ret

.section .init_array
.globl __init_array_end
__init_array_end:
	.quad 0
