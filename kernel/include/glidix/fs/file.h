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

#ifndef __glidix_fs_file_h
#define	__glidix_fs_file_h

#include <glidix/util/common.h>
#include <glidix/thread/mutex.h>
#include <glidix/fs/stat.h>
#include <glidix/fs/vfs.h>
#include <glidix/fs/path.h>

/**
 * File capability meaning it can be seeked.
 */
#define	VFS_FCAP_SEEKABLE					(1 << 0)

/**
 * An open file description.
 * 
 * The `oflags` field is at a fixed position (beginning of this struct), immutable,
 * and can be directly read by any code. However, all other fields are opaque, and
 * must only be accessed via functions provided in this file!
 */
typedef struct File_
{
	/**
	 * File open flags (`O_*`).
	 */
	int oflags;

	/**
	 * Reference count. This is an atomic field.
	 */
	int refcount;

	/**
	 * The path walker pointing to the inode this file is referring to.
	 */
	PathWalker walker;

	/**
	 * The mutex protecting access to the file.
	 */
	Mutex lock;

	/**
	 * File offset (only makes sense if `VFS_FCAP_SEEKABLE` is set).
	 */
	off_t offset;
} File;

/**
 * Create an open file description referring to an inode. Attempts to open with the specified oflags
 * (`O_*`); results in a `EACCES` error if we were not granted the appropriate access rights.
 * Returns the file description on success, or NULL on error. If NULL is returned, and `err` is not
 * NULL, then the error number is stored there.
 * 
 * If successful, this function takes its own reference to the inode.
 */
File* vfsOpenInode(PathWalker *walker, int oflags, errno_t *err);

/**
 * Increment the reference count of the file, and return it again.
 */
File* vfsDup(File *fp);

/**
 * Decrement the reference count of the file, and if it's now zero, clean up the file structures.
 */
void vfsClose(File *fp);

#endif