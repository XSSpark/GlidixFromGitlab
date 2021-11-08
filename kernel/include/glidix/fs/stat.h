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

#ifndef __glidix_fs_stat_h
#define	__glidix_fs_stat_h

#include <glidix/util/common.h>

#define	VFS_SEEK_SET					0
#define	VFS_SEEK_END					1
#define	VFS_SEEK_CUR					2

#define	VFS_MODE_SETUID					04000
#define	VFS_MODE_SETGID					02000
#define	VFS_MODE_STICKY					01000

#define	VFS_MODE_REGULAR				0		/* 0, so default */
#define	VFS_MODE_DIRECTORY				0x1000
#define	VFS_MODE_CHARDEV				0x2000
#define	VFS_MODE_BLKDEV					0x3000
#define	VFS_MODE_FIFO					0x4000
#define	VFS_MODE_LINK					0x5000		/* symlink */
#define	VFS_MODE_SOCKET					0x6000

#define	VFS_ACL_SIZE					128

/**
 * Mode type mask.
 */
#define	VFS_MODE_TYPEMASK				0xF000

#define	VFS_ACCESS_EXEC					(1 << 0)
#define	VFS_ACCESS_WRITE				(1 << 1)
#define	VFS_ACCESS_READ					(1 << 2)

/**
 * Inode flag indicating the inode can be seeked, i.e. it is a random-access file.
 */
#define	VFS_INODE_SEEKABLE				(1 << 0)

/**
 * Inode flag indicating the inode is only in RAM and thus cannot be cached when the
 * refcount is zero (this is only used by `ramfs`).
 */
#define	VFS_INODE_NOCACHE				(1 << 1)

/**
 * Dentry flag indicating the inode is only in RAM and thus cannot be cached when the
 * refcount is zero (this is only used by `ramfs`).
 */
#define	VFS_DENTRY_NOCACHE				(1 << 0)

/**
 * File open flags.
 */
#ifndef O_WRONLY
#define	O_WRONLY					(1 << 0)
#define	O_RDONLY					(1 << 1)
#define	O_RDWR						(O_WRONLY | O_RDONLY)
#define	O_APPEND					(1 << 2)
#define	O_CREAT						(1 << 3)
#define	O_EXCL						(1 << 4)
#define	O_NOCTTY					(1 << 5)
#define	O_TRUNC						(1 << 6)
#define	O_DSYNC						(1 << 7)
#define	O_NONBLOCK					(1 << 8)
#define	O_RSYNC						(1 << 9)
#define	O_SYNC						(1 << 10)
#define	O_CLOEXEC					(1 << 11)
#define	O_ACCMODE					(O_RDWR)
#define	O_ALL						(O_RDWR | O_APPEND | O_CREAT | O_EXCL | O_TRUNC | O_NOCTTY | O_NONBLOCK | O_CLOEXEC)
#endif

/**
 * File descriptor referring to the current working directory.
 */
#define	VFS_AT_FDCWD					0xFFFF

typedef struct
{
	uint16_t			ace_id;
	uint8_t				ace_type;
	uint8_t				ace_perms;
} AccessControlEntry;

struct kstat
{
	dev_t				st_dev;
	ino_t				st_ino;
	mode_t				st_mode;
	nlink_t				st_nlink;
	uid_t				st_uid;
	gid_t				st_gid;
	dev_t				st_rdev;
	off_t				st_size;
	blksize_t			st_blksize;
	blkcnt_t			st_blocks;
	time_t				st_atime;
	time_t				st_mtime;
	time_t				st_ctime;
	uint64_t			st_ixperm;
	uint64_t			st_oxperm;
	uint64_t			st_dxperm;
	time_t				st_btime;
	AccessControlEntry		st_acl[VFS_ACL_SIZE];
};

#endif