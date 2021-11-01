/*
	Glidix kernel

	Copyright (c) 2021, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __glidix_util_string_h
#define __glidix_util_string_h

#include <glidix/util/common.h>

/**
 * String and memory operations for the kernel. Implemented in string.asm and init.c.
 */

#define	ULONG_MAX							18446744073709551615UL

void   memcpy(void *dst, const void *src, size_t size);
void   memset(void *dst, char c, size_t size);
void   strcpy(char *dst, const char *src);
size_t strlen(const char *str);
int    memcmp(const void *a, const void *b, size_t size);
int    strcmp(const char *a, const char *b);
void   strcat(char *dst, const char *a);
char*  strdup(const char *str);
int    strncmp(const char *s1, const char *s2, size_t n);		/* init.c */
int    isprint(int c);							/* init.c */
char*  strncpy(char *s1, const char *s2, size_t n);			/* init.c */
int    isdigit (int c);							/* init.c */
int    isspace(int c);							/* init.c */
int    isxdigit(int c);							/* init.c */
int    toupper(int c);							/* init.c */
int    tolower(int c);                                                  /* init.c */
char*  strncat(char *dst, const char *src, size_t n);			/* init.c */
int    isalpha(int c);							/* init.c */
int    isupper(int c);							/* init.c */
int    islower(int c);							/* init.c */
unsigned long strtoul(const char *nptr, char **endptr, int base);	/* init.c */
char *strstr(const char *in, const char *str);				/* init.c */

/**
 * Performs string formatting. Similar to userspace sprintf(), but accepts a slightly different
 * format (just like the kernel kprintf()). Supported format specifiers:
 * %c (char) - outputs a literal character.
 * %s (const char *) - outputs a string
 * %d (int) - outputs a decimal integer.
 * Returns 0 on success, -1 on buffer overflow or format error.
 * Defined in strformat.c.
 */
int strformat(char *buffer, size_t bufsize, const char *format, ...);

#endif