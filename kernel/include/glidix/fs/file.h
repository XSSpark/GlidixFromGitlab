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
	 * The mutex protecting the offset field, and ensuring that read-write-seek
	 * operations are atomic.
	 */
	Mutex posLock;

	/**
	 * File offset.
	 */
	off_t offset;
} File;

/**
 * Create an open file description referring to an inode. Access rights are NOT checked.
 * Returns the file description on success, or NULL on error. If NULL is returned, and `err` is not
 * NULL, then the error number is stored there.
 * 
 * If successful, this function takes its own reference to the `walker`.
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

/**
 * Read from the specified position within a file. If the file is non-seekable, the position is ignored.
 * Returns the number of bytes successfully read, 0 on EOF, or a negated error number on error.
 */
ssize_t vfsPRead(File *fp, void *buffer, size_t size, off_t pos);

/**
 * Write to the specified position within a file. If the file is non-seekable, the position is ignored.
 * Returns the number of bytes successfully written, 0 on EOF, or a negated error number on error.
 */
ssize_t vfsPWrite(File *fp, const void *buffer, size_t size, off_t pos);

/**
 * Read from the current position within the file, and seek.
 */
ssize_t vfsRead(File *fp, void *buffer, size_t size);

/**
 * Write to the current position within the file, and seek.
 */
ssize_t vfsWrite(File *fp, const void *buffer, size_t size);

/**
 * Seek the file. `whence` is one of the `VFS_SEEK_*` constants. If successful, returns the new offset
 * (relative to the start of the file); otherwise returns a negated error number.
 */
off_t vfsSeek(File *fp, off_t offset, int whence);

#endif