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

#include <glidix/fs/path.h>
#include <glidix/fs/ramfs.h>
#include <glidix/util/log.h>
#include <glidix/util/panic.h>
#include <glidix/util/init.h>
#include <glidix/util/string.h>
#include <glidix/util/memory.h>

/**
 * The kernel root walker.
 */
static PathWalker vfsKernelRootWalker;

static void vfsInitKernelRoot()
{
	kprintf("Creating the kernel root directory...\n");

	errno_t err;
	FileSystem *rootfs = vfsCreateFileSystem("ramfs", "", NULL, &err);
	if (rootfs == NULL)
	{
		panic("Failed to create the ramfs for kernel root: errno %d", err);
	};

	vfsKernelRootWalker.current = vfsGetFileSystemRoot(rootfs, &err);
	if (vfsKernelRootWalker.current == NULL)
	{
		panic("Failed to get the kernel root: errno %d", err);
	};
};

KERNEL_INIT_ACTION(vfsInitKernelRoot, KAI_VFS_KERNEL_ROOT, KIA_RAMFS_REGISTER);

PathWalker vfsPathWalkerDup(PathWalker *walker)
{
	PathWalker newWalker;
	newWalker.current = vfsInodeDup(walker->current);
	return newWalker;
};

void vfsPathWalkerDestroy(PathWalker *walker)
{
	vfsInodeUnref(walker->current);
};

PathWalker vfsPathWalkerGetCurrentDir()
{
	// TODO: get the current working directory for the process, instead of always
	// defaulting to the kernel root!
	return vfsPathWalkerDup(&vfsKernelRootWalker);
};

PathWalker vfsPathWalkerGetRoot()
{
	// TODO: get the root directory for the process, instead of always defaulting
	// to the kernel root!
	return vfsPathWalkerDup(&vfsKernelRootWalker);
};

int vfsWalk(PathWalker *walker, const char *path)
{
	errno_t err;

	if (path[0] == 0)
	{
		// empty paths must not resolve
		return -ENOENT;
	};

	char *pbuf = strdup(path);
	if (pbuf == NULL) return -ENOMEM;

	char *scan = pbuf;
	if (*scan == '/')
	{
		// paths starts with /, go back to root
		scan++;
		vfsPathWalkerDestroy(walker);
		*walker = vfsPathWalkerGetRoot();
	};

	char *nextToken = scan;
	int isFinal = 0;
	while (!isFinal)
	{
		while (*scan != '/' && *scan != 0) scan++;

		if (*scan == 0)
		{
			isFinal = 1;
		}
		else
		{
			*scan++ = 0;
		};

		if ((walker->current->mode & VFS_MODE_TYPEMASK) != VFS_MODE_DIRECTORY)
		{
			kfree(pbuf);
			return -ENOTDIR;
		};

		if (!vfsInodeAccess(walker->current, VFS_ACCESS_EXEC))
		{
			// no search permission
			kfree(pbuf);
			return -EACCES;
		};

		if (nextToken[0] == 0)
		{
			// empty string, just continue
			continue;
		}
		else if (strcmp(nextToken, ".") == 0)
		{
			// the "." entry, points back to itself, so just continue
			continue;
		}
		else if (strcmp(nextToken, "..") == 0)
		{
			// the ".." entry, so go up
			Inode *nextInode = vfsInodeGet(walker->current->fs, walker->current->parentIno, &err);
			if (nextInode == NULL)
			{
				kfree(pbuf);
				return -err;
			};

			vfsInodeUnref(walker->current);
			walker->current = nextInode;
		}
		else
		{
			Dentry *dent = vfsDentryGet(walker->current, nextToken, &err);
			if (dent == NULL)
			{
				kfree(pbuf);
				return -err;
			};

			Inode *nextInode = vfsInodeGet(walker->current->fs, dent->target, &err);
			vfsDentryUnref(dent);

			if (nextInode == NULL)
			{
				kfree(pbuf);
				return -err;
			};

			vfsInodeUnref(walker->current);
			walker->current = nextInode;
		};
	};

	return 0;
};

char* vfsBaseName(const char *path)
{
	const char *scan = path;
	const char *baseNameComp = NULL;

	while (*scan != 0)
	{
		if (*scan == '/') baseNameComp = scan+1;
		scan++;
	};

	if (baseNameComp == NULL)
	{
		return strdup(path);
	}
	else
	{
		return strdup(baseNameComp);
	};
};

char* vfsDirName(const char *path)
{
	char *copy = strdup(path);
	if (copy == NULL) return NULL;

	char *scan = copy;
	char *lastSlash = NULL;

	while (*scan != 0)
	{
		if (*scan == '/') lastSlash = scan;
		scan++;
	};

	if (lastSlash == NULL)
	{
		// no slashes
		kfree(copy);
		return strdup(".");
	};

	*lastSlash = 0;
	if (copy[0] == 0)
	{
		// absolute path with dirname being "/"
		kfree(copy);
		return strdup("/");
	};

	return copy;
};

void vfsWalkToChild(PathWalker *walker, Inode *child)
{
	vfsInodeUnref(walker->current);
	walker->current = vfsInodeDup(child);
};