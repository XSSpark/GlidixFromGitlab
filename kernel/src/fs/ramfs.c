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

#include <glidix/fs/ramfs.h>
#include <glidix/fs/vfs.h>
#include <glidix/util/init.h>
#include <glidix/util/log.h>
#include <glidix/util/panic.h>
#include <glidix/hw/pagetab.h>
#include <glidix/util/string.h>

static ino_t ramfsNextIno = 8;

static int ramfsMount(FileSystem *fs, const char *image, const char *options)
{
	if (options != NULL)
	{
		// no options are supported
		return -EINVAL;
	};

	if (image[0] != 0)
	{
		// only an empty string is a valid image name
		return -EINVAL;
	};

	// otherwise success, there is nothing more to do
	return 0;
};

static ino_t ramfsGetRootIno(FileSystem *fs)
{
	return RAMFS_ROOT_INO;
};

static size_t ramfsGetInodeDriverDataSize(FileSystem *fs)
{
	return 0;
};

static int ramfsLoadInode(FileSystem *fs, Inode *inode, ino_t ino)
{
	if (ino != RAMFS_ROOT_INO)
	{
		// we should never be called with any other inode number
		panic("ramfsLoadInode called with inode number %lu!", ino);
	};

	// root directory is a directory, sticky bit set, read/write for root,
	// readonly for everyone else
	inode->mode = VFS_MODE_DIRECTORY | VFS_MODE_STICKY | 0755;

	// indicate that the inode is non-cacheable
	inode->flags = VFS_INODE_NOCACHE;

	// the root directory is its own parent
	inode->parentIno = RAMFS_ROOT_INO;
	
	return 0;
};

static int ramfsLoadDentry(Inode *parent, Dentry *dent)
{
	// if we were asked to load a dentry it means there was a cache miss,
	// which in the case of ramfs can only happen if the dentry does not
	// exist, so we return the error
	return -ENOENT;
};

static int ramfsMakeNode(Inode *parent, Dentry *dent, Inode *child)
{
	errno_t err;
	Dentry *collision = vfsDentryGet(parent, dent->name, &err);
	if (collision != NULL)
	{
		vfsDentryUnref(collision);
		return -EEXIST;
	};

	if (err != ENOENT)
	{
		return -err;
	};

	child->ino = __sync_fetch_and_add(&ramfsNextIno, 1);

	if ((child->mode & VFS_MODE_TYPEMASK) == VFS_MODE_REGULAR)
	{
		// regular file is seekable
		child->flags |= VFS_INODE_SEEKABLE;
	};

	dent->target = child->ino;
	dent->flags |= VFS_DENTRY_NOCACHE;

	return 0;
};

static int ramfsLoadPage(Inode *inode, off_t pos, void *buffer)
{
	// there's no data "already on disk" ever, so we just zero out
	memZeroPage(buffer);
	return 0;
};

/**
 * The ramfs FSDriver object.
 */
static FSDriver ramfsDriver = {
	.fsname = "ramfs",
	.mount = ramfsMount,
	.getRootIno = ramfsGetRootIno,
	.getInodeDriverDataSize = ramfsGetInodeDriverDataSize,
	.loadInode = ramfsLoadInode,
	.loadDentry = ramfsLoadDentry,
	.makeNode = ramfsMakeNode,
	.loadPage = ramfsLoadPage,
};

static void ramfsInit()
{
	kprintf("Registering the ramfs...\n");
	vfsRegisterFileSystemDriver(&ramfsDriver);
};

KERNEL_INIT_ACTION(ramfsInit, KIA_RAMFS_REGISTER, KAI_VFS_DRIVER_MAP);