/*
	TEMPORARY INIT
*/

#ifndef INIT_H_
#define INIT_H_

#include <stddef.h>
#include <inttypes.h>

#define	AT_FDCWD 0xFFFF

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

typedef int64_t ssize_t;

int openat(int dirfd, const char *path, int oflags, ...);
ssize_t write(int fd, const void *buffer, size_t size);
int dup3(int oldfd, int newfd, int cloexec);

#endif