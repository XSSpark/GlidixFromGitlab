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

/**
 * The mutex protecting the inode hashtable.
 */
static Mutex vfsInodeTableLock;

/**
 * The inode hashtable.
 */
static Inode* vfsInodeTable[VFS_INODETAB_NUM_BUCKETS];

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

FileSystem* vfsCreateFileSystem(FSDriver *driver, const char *image, const char *options, errno_t *err)
{
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

	inode->drvdata = inode->end;
	inode->flags = 0;
	inode->refcount = 1;
	inode->fs = fs;
	
	return inode;
};

Inode* vfsInodeGet(FileSystem *fs, ino_t ino, errno_t *err)
{
	int hash = vfsInodeHash(fs, ino) & (VFS_INODETAB_NUM_BUCKETS-1);
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
			};
		};
	};

	mutexUnlock(&vfsInodeTableLock);
	return inode;
};