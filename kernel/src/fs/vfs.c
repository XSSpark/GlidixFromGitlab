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
#include <glidix/hw/kom.h>
#include <glidix/hw/pagetab.h>

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
static unsigned int vfsInodeHash(FileSystem *fs, ino_t ino)
{
	return (unsigned int) (uint64_t) fs + (unsigned int) ino;
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
	unsigned int hash = vfsInodeHash(fs, ino) % VFS_INODETAB_NUM_BUCKETS;
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
static unsigned int vfsDentryHash(FileSystem *fs, ino_t parent, const char *name)
{
	unsigned int hash = (unsigned int) (uint64_t) fs + (unsigned int) parent;
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

	unsigned int hash = vfsDentryHash(dir->fs, dir->ino, name) % VFS_DENTRYTAB_NUM_BUCKETS;
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
	child->uid = schedGetEffectiveUID();
	child->gid = schedGetEffectiveGID();
	// TODO: other stuff like UID etc
};

static Inode* vfsCreateChildNode(Inode *parent, const char *basename, mode_t mode, errno_t *err)
{
	Inode *child = vfsAllocInode(parent->fs);
	if (child == NULL)
	{
		if (err != NULL) *err = ENOMEM;
		return NULL;
	};

	Dentry *dent = vfsAllocDentry(parent, basename);
	if (dent == NULL)
	{
		// TODO: proper uncaching etc of child
		kfree(child);
		if (err != NULL) *err = ENOMEM;
		return NULL;
	};

	vfsInodeInitAndInherit(parent, child, mode);

	mutexLock(&vfsInodeTableLock);
	mutexLock(&vfsDentryTableLock);

	int status = parent->fs->driver->makeNode(parent, dent, child);
	if (status == 0)
	{
		unsigned int ihash = vfsInodeHash(child->fs, child->ino) % VFS_INODETAB_NUM_BUCKETS;
		child->next = vfsInodeTable[ihash];
		if (child->next != NULL) child->next->prev = child;
		vfsInodeTable[ihash] = child;

		unsigned int dhash = vfsDentryHash(child->fs, parent->ino, dent->name) % VFS_DENTRYTAB_NUM_BUCKETS;
		dent->next = vfsDentryTable[dhash];
		if (dent->next != NULL) dent->next->prev = dent;
		vfsDentryTable[dhash] = dent;
	};

	mutexUnlock(&vfsDentryTableLock);
	mutexUnlock(&vfsInodeTableLock);

	if (status != 0)
	{
		// TODO: proper uncaching etc
		kfree(child);
		kfree(dent);
		if (err != NULL) *err = -status;
		return NULL;
	};

	vfsDentryUnref(dent);	
	return child;
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
		return status;
	};

	Inode *parent = vfsInodeDup(walker.current);
	vfsPathWalkerDestroy(&walker);

	if (!vfsInodeAccess(parent, VFS_ACCESS_WRITE | VFS_ACCESS_EXEC))
	{
		vfsInodeUnref(parent);
		kfree(basename);
		return -EACCES;
	};

	errno_t err;
	Inode *child = vfsCreateChildNode(parent, basename, (mode & ~vfsGetCurrentUmask() & 0777) | VFS_MODE_DIRECTORY, &err);
	kfree(basename);
	vfsInodeUnref(parent);

	if (child == NULL)
	{
		return -err;
	}
	else
	{
		vfsInodeUnref(child);
		return 0;
	};
};

struct File_* vfsOpen(struct File_ *start, const char *path, int oflags, mode_t mode, errno_t *err)
{
	if ((oflags & O_RDWR) == 0)
	{
		// neither the read nor the write flag was set
		if (err != NULL) *err = EINVAL;
		return NULL;
	};

	char *dirname = vfsDirName(path);
	if (dirname == NULL)
	{
		if (err != NULL) *err = ENOMEM;
		return NULL;
	};

	char *basename = vfsBaseName(path);
	if (basename == NULL)
	{
		kfree(dirname);
		if (err != NULL) *err = ENOMEM;
		return NULL;
	};

	PathWalker walker;
	if (start == NULL)
	{
		walker = vfsPathWalkerGetCurrentDir();
	}
	else
	{
		walker = vfsPathWalkerDup(&start->walker);
	};

	int status = vfsWalk(&walker, dirname);
	kfree(dirname);

	if (status != 0)
	{
		vfsPathWalkerDestroy(&walker);
		kfree(basename);
		if (err != NULL) *err = -status;
		return NULL;
	};

	int rights = VFS_ACCESS_EXEC;
	if (oflags & O_CREAT) rights |= VFS_ACCESS_WRITE;

	if (!vfsInodeAccess(walker.current, rights))
	{
		kfree(basename);
		if (err != NULL) *err = -status;
		vfsPathWalkerDestroy(&walker);
		return NULL;
	};

	errno_t derr;
	Dentry *dent = vfsDentryGet(walker.current, basename, &derr);
	if (err != NULL) *err = derr;

	Inode *child;
	if (dent == NULL && derr == ENOENT && (oflags & O_CREAT))
	{
		child = vfsCreateChildNode(walker.current, basename, mode & ~vfsGetCurrentUmask() & 0777, err);
	}
	else if (dent != NULL && (oflags & O_EXCL))
	{
		vfsDentryUnref(dent);
		if (err != NULL) *err = EEXIST;
		child = NULL;
	}
	else if (dent == NULL)
	{
		child = NULL;
	}
	else
	{
		child = vfsInodeGet(walker.current->fs, dent->target, err);
		if (child != NULL)
		{
			int rights = VFS_ACCESS_READ;
			if (oflags & O_WRONLY) rights |= VFS_ACCESS_WRITE;

			if (!vfsInodeAccess(child, rights))
			{
				vfsInodeUnref(child);
				if (err != NULL) *err = EACCES;
				child = NULL;
			};
		};
	};

	kfree(basename);

	if (child == NULL)
	{
		// we could not find/create the inode, err is already set
		vfsPathWalkerDestroy(&walker);
		return NULL;
	};

	// walk the walker to the child
	vfsWalkToChild(&walker, child);
	vfsInodeUnref(child);

	// try to open
	File *fp = vfsOpenInode(&walker, oflags, err);
	vfsPathWalkerDestroy(&walker);
	return fp;
};

/**
 * Get a pointer to the specified offset in the page cache of the specified inode.
 * Only call this while the page cache mutex is locked. The returned pointer can be
 * accessed as long as the mutex is held, but you must NOT cross any page boundaries!
 * 
 * If there is a cache miss, the page will be loaded. If it cannot be loaded, NULL
 * is returned, and if `err` is not NULL, it will be set to the error number.
 * 
 * If `markDirty` is nonzero, then the traversed pages will be marked dirty.
 */
static void* _vfsGetCachePage(Inode *inode, off_t offset, int markDirty, errno_t *err)
{
	if (offset < 0 || offset >= VFS_MAX_SIZE)
	{
		if (err != NULL) *err = EOVERFLOW;
		return NULL;
	};

	int indexes[4];
	indexes[3] = (offset >> 12) & 0x1FF;
	indexes[2] = (offset >> (12+9)) & 0x1FF;
	indexes[1] = (offset >> (12+9+9)) & 0x1FF;
	indexes[0] = (offset >> (12+9+9+9)) & 0x1FF;

	if (inode->pageCacheMaster == NULL)
	{
		inode->pageCacheMaster = (PageCacheNode*) komAllocBlock(
			KOM_BUCKET_PAGE, KOM_POOLBIT_ALL & ~(KOM_POOLBIT_INODES | KOM_POOLBIT_PAGE_CACHE)
		);

		if (inode->pageCacheMaster == NULL)
		{
			if (err != NULL) *err = ENOMEM;
			return NULL;
		};

		memset(inode->pageCacheMaster, 0, PAGE_SIZE);
	};

	PageCacheNode *node = inode->pageCacheMaster;
	int i;
	for (i=0; i<3; i++)
	{
		int indexIntoNode = indexes[i];
		if (node->ents[indexIntoNode] == 0)
		{
			PageCacheNode *nextNode = (PageCacheNode*) (PageCacheNode*) komAllocBlock(
				KOM_BUCKET_PAGE, KOM_POOLBIT_ALL & ~(KOM_POOLBIT_INODES | KOM_POOLBIT_PAGE_CACHE)
			);

			if (nextNode == NULL)
			{
				if (err != NULL) *err = ENOMEM;
				return NULL;
			};

			memset(nextNode, 0, PAGE_SIZE);

			node->ents[indexIntoNode] = (uint64_t) nextNode & VFS_PAGECACHE_ADDR_MASK;
		};

		if (markDirty)
		{
			node->ents[indexIntoNode] |= VFS_PAGECACHE_DIRTY;
		};

		node = (PageCacheNode*) (node->ents[indexIntoNode] | (~VFS_PAGECACHE_ADDR_MASK));
	};

	// final step is to get the page itself
	int finalIndex = indexes[3];
	if (node->ents[finalIndex] == 0)
	{
		// cache miss, try to load it
		off_t alignedOffset = offset & ~0xFFF;
		void *page = komAllocBlock(KOM_BUCKET_PAGE, KOM_POOLBIT_ALL & ~(KOM_POOLBIT_INODES | KOM_POOLBIT_PAGE_CACHE));
		
		if (page == NULL)
		{
			if (err != NULL) *err = ENOMEM;
			return NULL;
		};

		int status = inode->fs->driver->loadPage(inode, alignedOffset, page);
		if (status != 0)
		{
			komReleaseBlock(page, KOM_BUCKET_PAGE);
			if (err != NULL) *err = -status;
			return NULL;
		};

		node->ents[finalIndex] = (uint64_t) page & VFS_PAGECACHE_ADDR_MASK;
	};

	if (markDirty)
	{
		node->ents[finalIndex] |= VFS_PAGECACHE_DIRTY;
	};

	return (void*) ((node->ents[finalIndex] | (~VFS_PAGECACHE_ADDR_MASK)) + (offset & 0xFFF));
};

ssize_t vfsInodeRead(Inode *inode, void *buffer, size_t size, off_t pos)
{
	if (inode->ops != NULL)
	{
		return inode->ops->pread(inode, buffer, size, pos);
	}
	else if ((inode->mode & VFS_MODE_TYPEMASK) == VFS_MODE_REGULAR)
	{
		char *put = (char*) buffer;
		size_t totalFileSize = inode->size;
		if (pos+size > totalFileSize)
		{
			size = totalFileSize - pos;
		};

		mutexLock(&inode->pageCacheLock);

		ssize_t sizeReadGood = 0;
		errno_t err = 0;

		while (size > 0)
		{
			void *data = _vfsGetCachePage(inode, pos, 0, &err);
			if (data == NULL)
			{
				break;
			};

			size_t readNow = PAGE_SIZE - (pos & 0xFFF);
			if (readNow > size) readNow = size;

			memcpy(put, data, readNow);
			size -= readNow;
			put += readNow;
			sizeReadGood += readNow;
		};

		mutexUnlock(&inode->pageCacheLock);
		
		if (sizeReadGood == 0 && err != 0)
		{
			return -err;
		};

		return sizeReadGood;
	}
	else if ((inode->mode & VFS_MODE_TYPEMASK) == VFS_MODE_DIRECTORY)
	{
		return -EISDIR;
	}
	else
	{
		return -EINVAL;
	};
};

ssize_t vfsInodeWrite(Inode *inode, const void *buffer, size_t size, off_t pos)
{
	if (inode->ops != NULL)
	{
		return inode->ops->pwrite(inode, buffer, size, pos);
	}
	else if ((inode->mode & VFS_MODE_TYPEMASK) == VFS_MODE_REGULAR)
	{
		const char *scan = (const char*) buffer;

		size_t currentSize = inode->size;
		size_t newEnd = pos + size;

		while (currentSize < newEnd)
		{
			__sync_val_compare_and_swap(&inode->size, currentSize, newEnd);
			currentSize = inode->size;
		};

		mutexLock(&inode->pageCacheLock);

		ssize_t sizeWrittenGood = 0;
		errno_t err = 0;

		while (size > 0)
		{
			void *data = _vfsGetCachePage(inode, pos, 0, &err);
			if (data == NULL)
			{
				break;
			};

			size_t writeNow = PAGE_SIZE - (pos & 0xFFF);
			if (writeNow > size) writeNow = size;

			memcpy(data, scan, writeNow);
			size -= writeNow;
			scan += writeNow;
			sizeWrittenGood += writeNow;
		};

		mutexUnlock(&inode->pageCacheLock);

		if (sizeWrittenGood == 0 && err != 0)
		{
			return -err;
		};

		return sizeWrittenGood;
	}
	else if ((inode->mode & VFS_MODE_TYPEMASK) == VFS_MODE_DIRECTORY)
	{
		return -EISDIR;
	}
	else
	{
		return -EINVAL;
	};
};