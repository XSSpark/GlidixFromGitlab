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

#include <glidix/fs/file.h>
#include <glidix/util/memory.h>
#include <glidix/util/string.h>

File* vfsOpenInode(PathWalker *walker, int oflags, errno_t *err)
{
	File *fp = (File*) kmalloc(sizeof(File));
	if (fp == NULL)
	{
		if (err != NULL) *err = ENOMEM;
		return NULL;
	};

	fp->oflags = oflags;
	fp->refcount = 1;
	fp->walker = vfsPathWalkerDup(walker);
	mutexInit(&fp->posLock);
	fp->offset = 0;

	return fp;
};

File* vfsDup(File *fp)
{
	__sync_fetch_and_add(&fp->refcount, 1);

	return fp;
};

File* vfsFork(File *fp)
{
	File *newFP = (File*) kmalloc(sizeof(File));
	if (newFP == NULL) return NULL;

	newFP->oflags = fp->oflags;
	newFP->refcount = 1;
	newFP->walker = vfsPathWalkerDup(&fp->walker);
	mutexInit(&newFP->posLock);
	newFP->offset = fp->offset;

	return newFP;
};

void vfsClose(File *fp)
{
	if (__sync_add_and_fetch(&fp->refcount, -1) == 0)
	{
		vfsPathWalkerDestroy(&fp->walker);
		kfree(fp);
	};
};

ssize_t vfsPRead(File *fp, void *buffer, size_t size, off_t pos)
{
	if ((fp->oflags & O_RDONLY) == 0)
	{
		// not open for reading
		return -EBADF;
	};

	return vfsInodeRead(fp->walker.current, buffer, size, pos);
};

ssize_t vfsPWrite(File *fp, const void *buffer, size_t size, off_t pos)
{
	if ((fp->oflags & O_WRONLY) == 0)
	{
		// not open for writing
		return -EBADF;
	};

	return vfsInodeWrite(fp->walker.current, buffer, size, pos);
};

ssize_t vfsRead(File *fp, void *buffer, size_t size)
{
	if ((fp->walker.current->flags & VFS_INODE_SEEKABLE) == 0)
	{
		// not seekable, so don't take the lock (as non-seekable files may
		// block on reads)
		return vfsPRead(fp, buffer, size, 0);
	};

	if ((fp->oflags & O_RDONLY) == 0)
	{
		// not open for reading
		return -EBADF;
	};

	mutexLock(&fp->posLock);
	ssize_t result = vfsPRead(fp, buffer, size, fp->offset);
	if (result > 0)
	{
		fp->offset += result;
	};
	mutexUnlock(&fp->posLock);

	return result;
};

ssize_t vfsWrite(File *fp, const void *buffer, size_t size)
{
	if ((fp->walker.current->flags & VFS_INODE_SEEKABLE) == 0)
	{
		// not seekable
		return vfsPWrite(fp, buffer, size, 0);
	};

	if ((fp->oflags & O_WRONLY) == 0)
	{
		// not open for writing
		return -EBADF;
	};

	mutexLock(&fp->posLock);
	ssize_t result;
	if (fp->oflags & O_APPEND)
	{
		result = vfsPWrite(fp, buffer, size, fp->walker.current->size);
	}
	else
	{
		result = vfsPWrite(fp, buffer, size, fp->offset);
		if (result > 0)
		{
			fp->offset += result;
		};
	}
	mutexUnlock(&fp->posLock);

	return result;
};

off_t vfsSeek(File *fp, off_t offset, int whence)
{
	if ((fp->walker.current->flags & VFS_INODE_SEEKABLE) == 0)
	{
		// not seekable
		return -ESPIPE;
	};

	mutexLock(&fp->posLock);
	off_t target;
	switch (whence)
	{
	case VFS_SEEK_CUR:
		target = fp->offset + offset;
		break;
	case VFS_SEEK_END:
		target = fp->walker.current->size + offset;
		break;
	case VFS_SEEK_SET:
		target = offset;
		break;
	default:
		target = -1;
		break;
	};
	
	if (target >= 0 && target <= VFS_MAX_SIZE)
	{
		fp->offset = target;
	};

	mutexUnlock(&fp->posLock);

	if (target < 0)
	{
		return -EINVAL;
	}
	else if (target > VFS_MAX_SIZE)
	{
		return -EOVERFLOW;
	}
	else
	{
		return target;
	};
};