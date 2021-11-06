;	Glidix kernel
;
;	Copyright (c) 2021, Madd Games.
;	All rights reserved.
;	
;	Redistribution and use in source and binary forms, with or without
;	modification, are permitted provided that the following conditions are met:
;	
;	* Redistributions of source code must retain the above copyright notice, this
;	  list of conditions and the following disclaimer.
;	
;	* Redistributions in binary form must reproduce the above copyright notice,
;	  this list of conditions and the following disclaimer in the documentation
;	  and/or other materials provided with the distribution.
;	
;	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
;	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
;	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
;	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
;	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
;	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
;	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

section .text
bits 64

global memcpy
global memset
global strcpy
global strlen
global memcmp
global strcmp
global strcat
global memZeroPage

memcpy:
.copy16:
	cmp	rdx,	16
	jb	.copy8
	movups	xmm0,	[rsi]
	movups	[rdi],	xmm0
	add	rdi,	16
	add	rsi,	16
	sub	rdx,	16
	jmp	.copy16
.copy8:
	cmp	rdx,	8
	jb	.copy4
	mov	rax,	[rsi]
	mov	[rdi],	rax
	add	rdi,	8
	add	rsi,	8
	sub	rdx,	8
	jmp	.copy8
.copy4:
	cmp	rdx,	4
	jb	.copy2
	mov	eax,	[rsi]
	mov	[rdi],	eax
	add	rdi,	4
	add	rsi,	4
	sub	rdx,	4
	jmp	.copy4
.copy2:
	mov	rcx,	rdx
	test	rcx,	rcx
	jz	.return
	rep	movsb
.return:
	ret

memset:
	push	rbp
	mov	rbp,	rsp
	mov	rax,	rsi
	mov	rcx,	rdx
	rep	stosb
	pop	rbp
	ret

strcpy:
	lodsb
	test	al,	al
	stosb
	jnz	strcpy
	ret

strlen:
	xor	rcx,	rcx
	mov	rsi,	rdi
.next:
	lodsb
	test	al,	al
	jz	.end
	inc	rcx
	jmp	.next
.end:
	mov	rax,	rcx
	ret

memcmp:
	mov	rcx,	rdx
	test	rcx,	rcx
	jz	.ok
	;dec	rcx
.next:
	mov	al,	[rdi]
	mov	dl,	[rsi]
	inc	rdi
	inc	rsi
	cmp	al,	dl
	jnz	.not_equal
	loop	.next
.ok:
	xor	rax,	rax
	ret
.not_equal:
	mov	rax,	1
	ret

strcmp:
	mov	al,	[rdi]
	mov	dl,	[rsi]
	inc	rdi
	inc	rsi
	cmp	al,	dl
	jnz	.not_equal

	test	al,	al
	jz	.end

	test	dl,	dl
	jz	.end

	jmp strcmp
.not_equal:
	mov	rax,	1
	ret

.end:
	xor	rax,	rax
	ret

strcat:
	; find the NUL character in dst (rdi) and then just strcpy()
.next:
	mov	al,	[rdi]
	test	al,	al
	jz	strcpy
	inc	rdi
	jmp	.next

memZeroPage:
	xorps	xmm0,	xmm0
	mov	rcx,	256
.loop:
	movaps	[rdi],	xmm0
	add	rdi,	16
	loop	.loop
	ret