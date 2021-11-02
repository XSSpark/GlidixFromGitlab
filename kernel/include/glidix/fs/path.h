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

#ifndef __glidix_fs_path_h
#define	__glidix_fs_path_h

#include <glidix/util/common.h>
#include <glidix/fs/vfs.h>

/**
 * Kernel init action for creating the kernel root directory.
 */
#define	KAI_VFS_KERNEL_ROOT				"vfsInitKernelRoot"

/**
 * Represents the state of a path walk.
 */
typedef struct
{
	/**
	 * The directory inode we are currently at. We own the reference.
	 */
	Inode *current;
} PathWalker;

/**
 * Return a duplicate of the specified path walker.
 */
PathWalker vfsPathWalkerDup(PathWalker *walker);

/**
 * Destroy a path walker (unrefs all relevant inodes).
 */
void vfsPathWalkerDestroy(PathWalker *walker);

/**
 * Get a path walker referring to the current working directory.
 */
PathWalker vfsPathWalkerGetCurrentDir();

/**
 * Get a path walker referring to the root directory.
 */
PathWalker vfsPathWalkerGetRoot();

/**
 * Start at the specified path walker, and walk through `path` relative to it.
 * Returns 0 on success, or a negated error number on error.
 */
int vfsWalk(PathWalker *walker, const char *path);

/**
 * Get the base name component of the specified path, as a new string on the heap.
 * You must call `kfree()` on it later. Returns NULL if allocation failed.
 */
char* vfsBaseName(const char *path);

/**
 * Get the directory component of the specified path (everything except the basename).
 * You must call `kfree()` on it later, as the returning string is on the heap. Returns
 * NULL if allocation failed. Returns the string "." for a relative path with no slashes;
 * or "/" if the directory is the filesystem root.
 */
char* vfsDirName(const char *path);

#endif