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

#ifndef __glidix_int_exec_h
#define	__glidix_int_exec_h

#include <glidix/util/common.h>
#include <glidix/fs/file.h>

/**
 * Maximum number of bytes in an executable magic signature.
 */
#define	EXEC_SIG_MAX					16

/**
 * Describes an executable file format.
 */
typedef struct
{
	/**
	 * The "magic signature bytes"; if a file begins with these, assume it is of this
	 * executable format.
	 */
	uint8_t sig[EXEC_SIG_MAX];

	/**
	 * Number of bytes in `sig`.
	 */
	size_t sigSize;

	/**
	 * Implementation of `exec` for this executable format. Takes an open file pointer,
	 * which was already checked to be executable, to be executed. `path` is the path to
	 * `fp` (this is needed when executing scripts, for example). Also takes the command-line
	 * argument and environment variable pointers. On success, do not return, but simply
	 * jump into the new program. On error, return a negated error number.
	 * 
	 * This function is also responsibly for implemented the suid bit, since it is not
	 * possible to implement it any other way.
	 * 
	 * If an error occurs, do not close the file. If successful, this function is responsible
	 * for ensuring the file is cleaned up.
	 */
	int (*doExec)(File *fp, const char *path, const char **argv, const char **envp);
} ExecFileFormat;

/**
 * Execute the specified file, passing it the specified command-line arguments, and environment
 * variables. On success, does not return, and jumps into the new program. On error, returns a
 * negated error number.
 */
int kexec(const char *path, const char **argv, const char **envp);

#endif