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

/**
 * Number of buckets in the dentry hashtable.
 */
#define	VFS_DENTRYTAB_NUM_BUCKETS			512

/**
 * Kernel init action for setting up the VFS driver system.
 */
#define	KAI_VFS_DRIVER_MAP				"vfsInitDriverMap"

/**
 * The dirty flag in page cache.
 */
#define	VFS_PAGECACHE_DIRTY				(1UL << 63)

/**
 * Maximum size of a file. File offsets can only be up to 48 bits long,
 * just like memory addresses.
 */
#define	VFS_MAX_SIZE					(1UL << 48)

/**
 * Address mask in the page cache.
 */
#define	VFS_PAGECACHE_ADDR_MASK				0x0000FFFFFFFFFFFFUL

// typedef all the structs here
typedef struct FSDriver_ FSDriver;
typedef struct FileSystem_ FileSystem;
typedef struct InodeOps_ InodeOps;
typedef struct Inode_ Inode;
typedef struct Dentry_ Dentry;
struct File_;

/**
 * Page cache node.
 */
typedef struct
{
	/**
	 * Flags and address. The bottom 48 bits can be sign-extended
	 * (and are always negative!) to get the address of the next node,
	 * while the top 16 bits are used for flags.
	 */
	uint64_t ents[512];
} PageCacheNode;

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
	 * 
	 * The `ino` is guaranteed to be a value provided by the driver itself; this will either be
	 * the return value of `getRootIno()`, or an inode number we got from a dentry.
	 */
	int (*loadInode)(FileSystem *fs, Inode *inode, ino_t ino);

	/**
	 * Load a dentry when there was a dentry cache miss. If successful, this function sets `dent->target`
	 * to the target inode number, and returns 0. Otherwise, it returns a negated error number.
	 */
	int (*loadDentry)(Inode *inode, Dentry *dent);

	/**
	 * Make a new inode in the filesystem. `parent` is the inode of the parent directory. `dent` is a new
	 * dentry (with `dent->name` being set) to be added to the parent directory, and `child` is the child
	 * inode. If this operation is successful, it must set `child->ino` and `dent->target` to be the
	 * newly-allocated inode number for the child inode, and then return 0. On error, it must return a
	 * negated error number. If a dentry with the specified name already exists in the parent directory on
	 * disk, this function must fail! This function must also check the inode type in `child->mode`, to
	 * ensure that the type of recognised and that the underlying filesystem can create it properly.
	 */
	int (*makeNode)(Inode *parent, Dentry *dent, Inode *child);

	/**
	 * Load the specified page from `inode` into `buffer`. `offset` is a page-aligned offset into the file
	 * data. Returns 0 on success, or a negated error number on error.
	 * 
	 * If the offset does not currently exist, most filesystem will zero out `buffer`.
	 */
	int (*loadPage)(Inode *inode, off_t offset, void *buffer);
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
 * Special inode operations.
 */
struct InodeOps_
{
	/**
	 * Read from the specified position within the file. If the file is non-seekable,
	 * ignore the position. Return the number of bytes successfully read, 0 on EOF,
	 * or a negated error number on error.
	 */
	ssize_t (*pread)(Inode *inode, void *buffer, size_t size, off_t pos);

	/**
	 * Write to the specified position within the file. If the file is non-seekable,
	 * ignore the position. Returns the number of bytes successfully written, or a
	 * negated error number on error.
	 */
	ssize_t (*pwrite)(Inode *inode, const void *buffer, size_t size, off_t pos);
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
	 * If this is not NULL, then this contains pointers to implementations of file operations
	 * for special files (such as devices, pipes etc). If this is NULL, then regular operations
	 * are implemented instead (using the page cache, etc).
	 */
	InodeOps *ops;

	/**
	 * The filesystem description for the filesystem on which this inode resides.
	 */
	FileSystem *fs;

	/**
	 * The inode number.
	 */
	ino_t ino;

	/**
	 * The mode.
	 */
	mode_t mode;

	/**
	 * Size of the file (for regular files).
	 */
	size_t size;

	/**
	 * Owner of the inode.
	 */
	uid_t uid;

	/**
	 * Group associated with the inode.
	 */
	gid_t gid;

	/**
	 * Mutex protecting the page cache.
	 */
	Mutex pageCacheLock;

	/**
	 * Master node of the page cache (may be NULL).
	 */
	PageCacheNode *pageCacheMaster;

	/**
	 * Char array at the end, this is where `drvdata` will be allocated.
	 */
	char end[];
};

/**
 * A directory entry.
 */
struct Dentry_
{
	/**
	 * KOM object header.
	 */
	KOM_Header header;

	/**
	 * Dentry flags (`VFS_DENTRY_*`).
	 */
	int flags;

	/**
	 * Reference count.
	 */
	int refcount;

	/**
	 * Links within the dentry hashtable.
	 */
	Dentry *prev;
	Dentry *next;
	
	/**
	 * The filesystem containing this dentry.
	 */
	FileSystem *fs;

	/**
	 * The parent inode number (i.e. the directory containing this dentry).
	 */
	ino_t parent;

	/**
	 * The inode number of the dentry target.
	 */
	ino_t target;

	/**
	 * The name.
	 */
	char name[];
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
FileSystem* vfsCreateFileSystem(const char *fsname, const char *image, const char *options, errno_t *err);

/**
 * Get an inode from the specified filesystem, and increment its reference count. You must call
 * `vfsInodeUnref()` on the returned inode later. On error, returns NULL; if `err` is not NULL,
 * stores the error number there.
 */
Inode* vfsInodeGet(FileSystem *fs, ino_t ino, errno_t *err);

/**
 * Register a new filesystem driver. Returns 0 on success, or an error number on error.
 */
errno_t vfsRegisterFileSystemDriver(FSDriver *driver);

/**
 * Get (and upref) the root inode of the specified filesystem. Returns NULL on error; and if `err`
 * is not NULL, it is set to the error number.
 */
Inode* vfsGetFileSystemRoot(FileSystem *fs, errno_t *err);

/**
 * Increment the refcount of a dentry and return it again.
 */
Dentry* vfsDentryDup(Dentry *dent);

/**
 * Unreference a dentry.
 */
void vfsDentryUnref(Dentry *dent);

/**
 * Get (and upref) a dentry on the specified directory, with the specified name. Returns NULL on error;
 * and if `err` is not NULL, it is set to the error number.
 */
Dentry* vfsDentryGet(Inode *dir, const char *name, errno_t *err);

/**
 * Create a new directory. `fp` is a file pointer referring to the directory which will be used as the
 * starting point for relative paths; or NULL if the current working directory should be used. `path`
 * is a path for the new directory (the parent must already exist). `mode` specifies the permissions
 * with which the directory is to be created. Returns 0 on success, or a negated error number on error.
 */
int vfsCreateDirectory(struct File_ *fp, const char *path, mode_t mode);

/**
 * Open (and possibly create) a file. `start` is a file pointer referring to the directory which will
 * be used as the starting point for relative paths; or NULL if the current working directory should
 * be used. Returns a file pointer on success, or NULL on error. If `err` is not NULL, the error number
 * will be stored there.
 */
struct File_* vfsOpen(struct File_ *start, const char *path, int oflags, mode_t mode, errno_t *err);

/**
 * Read from the specified position within an inode. If the inode is non-seekable, the position is
 * ignored. Returns the number of bytes successfully read, 0 on EOF, or a negated error number on error.
 */
ssize_t vfsInodeRead(Inode *inode, void *buffer, size_t size, off_t pos);

/**
 * Write to the specified position within an inode. If the inode is non-seekable, the position is
 * ignored. Returns the numbe of bytes successfully written, or a negated error number on error.
 */
ssize_t vfsInodeWrite(Inode *inode, const void *buffer, size_t size, off_t pos);

#endif