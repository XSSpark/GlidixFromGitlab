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

#include <glidix/util/memory.h>
#include <glidix/fs/vfs.h>
#include <glidix/util/hashmap.h>
#include <glidix/util/init.h>
#include <glidix/util/panic.h>
#include <glidix/util/log.h>
#include <glidix/util/string.h>
#include <glidix/fs/ramfs.h>
#include <glidix/fs/file.h>
#include <glidix/fs/path.h>

/**
 * The mutex protecting the inode hashtable.
 */
static Mutex vfsInodeTableLock;

/**
 * The inode hashtable.
 */
static Inode* vfsInodeTable[VFS_INODETAB_NUM_BUCKETS];

/**
 * The mutex protecting the filesystem driver map.
 */
static Mutex vfsDriverMapLock;

/**
 * The filesystem driver map.
 */
static HashMap* vfsDriverMap;

/**
 * The mutex protecting the dentry hashtable.
 */
static Mutex vfsDentryTableLock;

/**
 * The dentry hashtable.
 */
static Dentry* vfsDentryTable[VFS_DENTRYTAB_NUM_BUCKETS];

static void vfsInitDriverMap()
{
	kprintf("Creating the filesystem driver map...\n");
	
	vfsDriverMap = hmNew();
	if (vfsDriverMap == NULL)
	{
		panic("Failed to allocate the filesystem driver map!");
	};
};

KERNEL_INIT_ACTION(vfsInitDriverMap, KAI_VFS_DRIVER_MAP);

int vfsInodeAccess(Inode *inode, int rights)
{
	// TODO -- for now we are granting all rights!
	return 1;
};

Inode* vfsInodeDup(Inode *inode)
{
	__sync_add_and_fetch(&inode->refcount, 1);

	return inode;
};

void vfsInodeUnref(Inode *inode)
{
	if (__sync_add_and_fetch(&inode->refcount, -1) == 0)
	{
		// TODO
	};
};

FileSystem* vfsCreateFileSystem(const char *fsname, const char *image, const char *options, errno_t *err)
{
	mutexLock(&vfsDriverMapLock);
	FSDriver *driver = hmGet(vfsDriverMap, fsname);
	mutexUnlock(&vfsDriverMapLock);

	if (driver == NULL)
	{
		if (err != NULL) *err = EINVAL;
		return NULL;
	};

	FileSystem *fs = (FileSystem*) kmalloc(sizeof(FileSystem));
	if (fs == NULL)
	{
		if (err != NULL) *err = ENOMEM;
		return NULL;
	};

	fs->driver = driver;
	
	errno_t status = driver->mount(fs, image, options);
	if (status != 0)
	{
		kfree(fs);
		if (err != NULL) *err = status;
		return NULL;
	};

	return fs;
};

/**
 * Calculate the hash of a filesystem/inode number for the inode hashtable.
 */
static int vfsInodeHash(FileSystem *fs, ino_t ino)
{
	return (int) (uint64_t) fs + (int) ino;
};

static Inode* vfsAllocInode(FileSystem *fs)
{
	Inode *inode = (Inode*) kmalloc(sizeof(Inode) + fs->driver->getInodeDriverDataSize(fs));
	if (inode == NULL)
	{
		return NULL;
	};

	memset(inode, 0, sizeof(Inode));
	inode->drvdata = inode->end;
	inode->flags = 0;
	inode->refcount = 1;
	inode->fs = fs;
	
	return inode;
};

Inode* vfsInodeGet(FileSystem *fs, ino_t ino, errno_t *err)
{
	int hash = vfsInodeHash(fs, ino) % VFS_INODETAB_NUM_BUCKETS;
	Inode *inode;

	mutexLock(&vfsInodeTableLock);

	for (inode=vfsInodeTable[hash]; inode!=NULL; inode=inode->next)
	{
		if (inode->fs == fs && inode->ino == ino)
		{
			__sync_add_and_fetch(&inode->refcount, 1);
			break;
		};
	};

	if (inode == NULL)
	{
		inode = vfsAllocInode(fs);
		if (err != NULL) *err = ENOMEM;

		if (inode != NULL)
		{
			inode->ino = ino;

			int status = fs->driver->loadInode(fs, inode, ino);
			if (status != 0)
			{
				// TODO: release inode correctly
				kfree(inode);
				if (err != NULL) *err = -status;
				inode = NULL;
			}
			else
			{
				inode->next = vfsInodeTable[hash];
				if (inode->next != NULL) inode->next->prev = inode;
				vfsInodeTable[hash] = inode;
			}
		};
	};

	mutexUnlock(&vfsInodeTableLock);
	return inode;
};

errno_t vfsRegisterFileSystemDriver(FSDriver *driver)
{
	mutexLock(&vfsDriverMapLock);
	errno_t err = 0;

	if (hmGet(vfsDriverMap, driver->fsname) != NULL)
	{
		err = EEXIST;
	}
	else if (hmSet(vfsDriverMap, driver->fsname, driver) != 0)
	{
		err = ENOMEM;
	};

	mutexUnlock(&vfsDriverMapLock);
	return err;
};

Inode* vfsGetFileSystemRoot(FileSystem *fs, errno_t *err)
{
	return vfsInodeGet(fs, fs->driver->getRootIno(fs), err);
};

/**
 * Calculate the hash of a dentry, for lookup on the dentry hashtable.
 */
static int vfsDentryHash(FileSystem *fs, ino_t parent, const char *name)
{
	int hash = (int) (uint64_t) fs + (int) parent;
	while (*name != 0)
	{
		hash <<= 7;
		hash ^= *name++;
	};

	return hash;
};

Dentry* vfsDentryDup(Dentry *dent)
{
	__sync_add_and_fetch(&dent->refcount, 1);
	return dent;
};

void vfsDentryUnref(Dentry *dent)
{
	__sync_add_and_fetch(&dent->refcount, -1);
	// TODO: release or something
};

static Dentry* vfsAllocDentry(Inode *dir, const char *name)
{
	Dentry *dent = (Dentry*) kmalloc(sizeof(Dentry) + strlen(name) + 1);
	if (dent == NULL) return NULL;

	dent->prev = NULL;
	dent->next = NULL;
	dent->fs = dir->fs;
	dent->flags = 0;
	dent->refcount = 1;
	dent->parent = dir->ino;
	strcpy(dent->name, name);

	return dent;
};

Dentry* vfsDentryGet(Inode *dir, const char *name, errno_t *err)
{
	if ((dir->mode & VFS_MODE_TYPEMASK) != VFS_MODE_DIRECTORY)
	{
		if (err != NULL) *err = ENOTDIR;
		return NULL;
	};

	int hash = vfsDentryHash(dir->fs, dir->ino, name) % VFS_DENTRYTAB_NUM_BUCKETS;
	mutexLock(&vfsDentryTableLock);

	Dentry *dent;
	for (dent=vfsDentryTable[hash]; dent!=NULL; dent=dent->next)
	{
		if (dent->fs == dir->fs && dent->parent == dir->ino && strcmp(dent->name, name) == 0)
		{
			vfsDentryDup(dent);
			break;
		};
	};

	if (dent == NULL)
	{
		dent = vfsAllocDentry(dir, name);
		if (err != NULL) *err = ENOMEM;

		if (dent != NULL)
		{
			int status = dir->fs->driver->loadDentry(dir, dent);
			if (status != 0)
			{
				// TODO: free dentry from cache properly
				kfree(dent);
				if (err != NULL) *err = -status;
				dent = NULL;
			}
			else
			{
				dent->next = vfsDentryTable[hash];
				if (dent->next != NULL) dent->next->prev = dent;
				vfsDentryTable[hash] = dent;
			};
		};
	};

	mutexUnlock(&vfsDentryTableLock);
	return dent;
};

static mode_t vfsGetCurrentUmask()
{
	// TODO: get it from the current process!
	return 0;
};

static void vfsInodeInitAndInherit(Inode *parent, Inode *child, mode_t mode)
{
	child->mode = mode;
	// TODO: other stuff like UID etc
};

int vfsCreateDirectory(File *fp, const char *path, mode_t mode)
{
	char *dirname = vfsDirName(path);
	if (dirname == NULL)
	{
		return -ENOMEM;
	};

	char *basename = vfsBaseName(path);
	if (basename == NULL)
	{
		kfree(dirname);
		return -ENOMEM;
	};

	PathWalker walker;
	if (fp == NULL)
	{
		walker = vfsPathWalkerGetCurrentDir();
	}
	else
	{
		walker = vfsPathWalkerDup(&fp->walker);
	};

	int status = vfsWalk(&walker, dirname);
	kfree(dirname);

	if (status != 0)
	{
		vfsPathWalkerDestroy(&walker);
		kfree(basename);
		return -status;
	};

	Inode *parent = vfsInodeDup(walker.current);
	vfsPathWalkerDestroy(&walker);

	if (!vfsInodeAccess(parent, VFS_ACCESS_WRITE))
	{
		vfsInodeUnref(parent);
		kfree(basename);
		return -EACCES;
	};

	Inode *child = vfsAllocInode(parent->fs);
	if (child == NULL)
	{
		vfsInodeUnref(parent);
		kfree(basename);
		return -ENOMEM;
	};

	Dentry *dent = vfsAllocDentry(parent, basename);
	kfree(basename);

	if (dent == NULL)
	{
		vfsInodeUnref(parent);
		// TODO: proper uncaching etc of child
		kfree(child);
		return -ENOMEM;
	};

	vfsInodeInitAndInherit(parent, child, (mode & ~vfsGetCurrentUmask() & 0777) | VFS_MODE_DIRECTORY);

	mutexLock(&vfsInodeTableLock);
	mutexLock(&vfsDentryTableLock);

	status = parent->fs->driver->makeNode(parent, dent, child);
	if (status == 0)
	{
		int ihash = vfsInodeHash(child->fs, child->ino) % VFS_INODETAB_NUM_BUCKETS;
		child->next = vfsInodeTable[ihash];
		if (child->next != NULL) child->next->prev = child;
		vfsInodeTable[ihash] = child;

		int dhash = vfsDentryHash(child->fs, parent->ino, dent->name) % VFS_DENTRYTAB_NUM_BUCKETS;
		dent->next = vfsDentryTable[dhash];
		if (dent->next != NULL) dent->next->prev = dent;
		vfsDentryTable[dhash] = dent;
	};

	mutexUnlock(&vfsDentryTableLock);
	mutexUnlock(&vfsInodeTableLock);

	vfsInodeUnref(parent);

	if (status != 0)
	{
		// TODO: proper uncaching etc
		kfree(child);
		kfree(dent);
	};

	vfsInodeUnref(child);
	vfsDentryUnref(dent);
	
	return status;
};