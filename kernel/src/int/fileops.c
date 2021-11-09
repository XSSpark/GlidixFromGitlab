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

#include <glidix/int/fileops.h>
#include <glidix/fs/file.h>
#include <glidix/thread/process.h>
#include <glidix/util/memory.h>

int sys_openat(int dirfd, user_addr_t upath, int oflags, mode_t mode)
{
	File *startdir;
	char path[PROC_USER_STRING_SIZE];

	int status = procReadUserString(path, upath);
	if (status != 0)
	{
		return status;
	};

	int fd = procFileResv();
	if (fd == -1)
	{
		return -EMFILE;
	};

	if (dirfd == VFS_AT_FDCWD)
	{
		startdir = NULL;
	}
	else
	{
		startdir = procFileGet(dirfd);
		if (startdir == NULL)
		{
			procFileSet(fd, NULL, 0);
			return -EBADF;
		};
	};

	errno_t err;
	File *fp = vfsOpen(startdir, path, oflags, mode, &err);
	if (startdir != NULL) vfsClose(startdir);

	if (fp == NULL)
	{
		procFileSet(fd, NULL, 0);
		return -err;
	};

	procFileSet(fd, fp, oflags & O_CLOEXEC);
	vfsClose(fp);		// unref our handle (`procFileSet` takes its own)

	return fd;
};

int sys_close(int fd)
{
	return procFileClose(fd);
};

ssize_t sys_read(int fd, user_addr_t ubuffer, size_t size)
{
	if (size > SYS_FILEOP_BUFFER_MAX) size = SYS_FILEOP_BUFFER_MAX;
	void *buffer = kmalloc(size);

	if (buffer == NULL)
	{
		return -ENOMEM;
	};

	File *fp = procFileGet(fd);
	if (fp == NULL)
	{
		kfree(buffer);
		return -EBADF;
	};

	ssize_t result = vfsRead(fp, buffer, size);
	if (result > 0)
	{
		int status = procToUserCopy(ubuffer, buffer, result);
		if (status != 0)
		{
			kfree(buffer);
			return -status;
		};
	};

	kfree(buffer);
	vfsClose(fp);
	return result;
};

ssize_t sys_write(int fd, user_addr_t ubuffer, size_t size)
{
	if (size > SYS_FILEOP_BUFFER_MAX) size = SYS_FILEOP_BUFFER_MAX;
	void *buffer = kmalloc(size);

	if (buffer == NULL)
	{
		return -ENOMEM;
	};

	int status = procToKernelCopy(buffer, ubuffer, size);
	if (status != 0)
	{
		kfree(buffer);
		return status;
	};

	File *fp = procFileGet(fd);
	if (fp == NULL)
	{
		kfree(buffer);
		return -EBADF;
	};

	ssize_t result = vfsWrite(fp, buffer, size);
	kfree(buffer);
	vfsClose(fp);

	return result;
};

ssize_t sys_pread(int fd, user_addr_t ubuffer, size_t size, off_t offset)
{
	if (size > SYS_FILEOP_BUFFER_MAX) size = SYS_FILEOP_BUFFER_MAX;
	void *buffer = kmalloc(size);

	if (buffer == NULL)
	{
		return -ENOMEM;
	};

	File *fp = procFileGet(fd);
	if (fp == NULL)
	{
		kfree(buffer);
		return -EBADF;
	};

	ssize_t result = vfsPRead(fp, buffer, size, offset);
	if (result > 0)
	{
		int status = procToUserCopy(ubuffer, buffer, result);
		if (status != 0)
		{
			kfree(buffer);
			return -status;
		};
	};

	kfree(buffer);
	vfsClose(fp);
	return result;
};

ssize_t sys_pwrite(int fd, user_addr_t ubuffer, size_t size, off_t offset)
{
	if (size > SYS_FILEOP_BUFFER_MAX) size = SYS_FILEOP_BUFFER_MAX;
	void *buffer = kmalloc(size);

	if (buffer == NULL)
	{
		return -ENOMEM;
	};

	int status = procToKernelCopy(buffer, ubuffer, size);
	if (status != 0)
	{
		kfree(buffer);
		return status;
	};

	File *fp = procFileGet(fd);
	if (fp == NULL)
	{
		kfree(buffer);
		return -EBADF;
	};

	ssize_t result = vfsPWrite(fp, buffer, size, offset);
	kfree(buffer);
	vfsClose(fp);

	return result;
};