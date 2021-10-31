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

#ifndef __glidix_fs_vfs_h
#define	__glidix_fs_vfs_h

#include <glidix/util/common.h>
#include <glidix/hw/kom.h>
#include <glidix/fs/stat.h>
#include <glidix/thread/mutex.h>

/**
 * Number of buckets in the inode hashtable.
 */
#define	VFS_INODETAB_NUM_BUCKETS			128

// typedef all the structs here
typedef struct FSDriver_ FSDriver;
typedef struct FileSystem_ FileSystem;
typedef struct Inode_ Inode;

/**
 * A filesystem driver.
 */
struct FSDriver_
{
	/**
	 * Name of the file system.
	 */
	const char *fsname;

	/**
	 * Called when a filesystem of this type is being mounted. If successful, you can
	 * set the `drvdata` field of the `fs`. Return 0 on success, or a negated error number
	 * on error.
	 */
	int (*mount)(FileSystem *fs, const char *image, const char *options);

	/**
	 * Get the inode number for the root directory.
	 */
	ino_t (*getRootIno)(FileSystem *fs);

	/**
	 * Get the size of the inode data struct.
	 */
	size_t (*getInodeDriverDataSize)(FileSystem *fs);

	/**
	 * Load an inode. The `inode` structure is initialized, and `drvdata` points to a block
	 * of data of the size returned by `getInodeDriverDataSize()`, and the driver must initialize
	 * it. Return 0 if successful, or a negated error number on error.
	 */
	int (*loadInode)(FileSystem *fs, Inode *inode, ino_t ino);
};

/**
 * A filesystem description.
 */
struct FileSystem_
{
	/**
	 * Driver-specific data.
	 */
	void *drvdata;

	/**
	 * The filesystem driver.
	 */
	FSDriver *driver;
};

/**
 * An inode, containing information about a filesystem member.
 */
struct Inode_
{
	/**
	 * KOM object header.
	 */
	KOM_Header header;

	/**
	 * Driver-specific data.
	 */
	void *drvdata;

	/**
	 * The inode flags (`VFS_INODE_*`).
	 */
	int flags;

	/**
	 * Reference count.
	 */
	int refcount;

	/**
	 * Links in the inode table.
	 */
	Inode *prev;
	Inode *next;

	/**
	 * The filesystem description for the filesystem on which this inode resides.
	 */
	FileSystem *fs;

	/**
	 * The inode number.
	 */
	ino_t ino;

	/**
	 * Char array at the end, this is where `drvdata` will be allocated.
	 */
	char end[];
};

/**
 * Determine if the specified access rights (bitwise-OR of one or more `VFS_ACCESS_*`) can be
 * peformed by the current user. Returns nonzero if access granted.
 */
int vfsInodeAccess(Inode *inode, int rights);

/**
 * Increment the reference count of an inode and return the inode itself.
 */
Inode* vfsInodeDup(Inode *inode);

/**
 * Decrement the reference count of an inode. If the refcount reaches 0, cleans up the inode,
 * possibly storing it in a cache.
 */
void vfsInodeUnref(Inode *inode);

/**
 * Create a filesystem description. This is called when a filesystem is being mounted. Returns
 * the filesystem description on success, or NULL on error. If `err` is not NULL and this function
 * returns NULL, the error number is stored there.
 * 
 * `image` is a path to a filesystem image. The meaning of this depends on the exact filesystem.
 * `options` is a string which contains options understood by the filesystem driver. The format is
 * driver-specific. `options` is explicitly allowed to be set to NULL for default options.
 */
FileSystem* vfsCreateFileSystem(FSDriver *driver, const char *image, const char *options, errno_t *err);

/**
 * Get an inode from the specified filesystem, and increment its reference count. You must call
 * `vfsInodeUnref()` on the returned inode later. On error, returns NULL; if `err` is not NULL,
 * stores the error number there.
 */
Inode* vfsInodeGet(FileSystem *fs, ino_t ino, errno_t *err);

#endif