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

#include <glidix/int/exec.h>
#include <glidix/int/elf64.h>
#include <glidix/util/string.h>

/**
 * List of supported executable file formats.
 */
static ExecFileFormat execFormats[] = {
	{"\x7f" "ELF", 4, elfExec},

	// LIST TERMINATOR
	{"", 0, NULL},
};

int kexec(const char *path, const char **argv, const char **envp)
{
	errno_t err;
	File *fp = vfsOpen(NULL, path, O_RDONLY, 0, &err);
	if (fp == NULL)
	{
		return -err;
	};

	if (!vfsInodeAccess(fp->walker.current, VFS_ACCESS_EXEC))
	{
		vfsClose(fp);
		return -EACCES;
	};

	uint8_t sig[EXEC_SIG_MAX];
	ssize_t sz = vfsPRead(fp, sig, EXEC_SIG_MAX, 0);

	if (sz < 0)
	{
		vfsClose(fp);
		return sz;
	};

	ExecFileFormat *format;
	for (format=execFormats; format->doExec!=NULL; format++)
	{
		if (sz >= format->sigSize && memcmp(format->sig, sig, format->sigSize) == 0)
		{
			break;
		};
	};

	if (format->doExec == NULL)
	{
		vfsClose(fp);
		return -ENOEXEC;
	};

	int status = format->doExec(fp, path, argv, envp);
	vfsClose(fp);
	return status;
};