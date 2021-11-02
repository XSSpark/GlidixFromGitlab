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
	mutexInit(&fp->lock);
	fp->offset = 0;

	return fp;
};

File* vfsDup(File *fp)
{
	__sync_fetch_and_add(&fp->refcount, 1);

	return fp;
};

void vfsClose(File *fp)
{
	if (__sync_add_and_fetch(&fp->refcount, -1) == 0)
	{
		vfsPathWalkerDestroy(&fp->walker);
		kfree(fp);
	};
};