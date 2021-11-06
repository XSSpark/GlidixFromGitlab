/*
	Glidix Runtime

	Copyright (c) 2014-2017, Madd Games.
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

#include <sys/glidix.h>
#include <sys/call.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>

int fcntl(int fd, int cmd, ...)
{
	va_list ap;
	va_start(ap, cmd);

	switch (cmd)
	{
	case F_DUPFD:
		return dup2(fd, va_arg(ap, int));
	case F_GETFD:
		return _glidix_fcntl_getfd(fd);
	case F_SETFD:
		return _glidix_fcntl_setfd(fd, va_arg(ap, int));
	case F_GETFL:
		return __syscall(__SYS_fcntl_getfl, fd);
	case F_SETFL:
		return __syscall(__SYS_fcntl_setfl, fd, va_arg(ap, int));
	case F_GETLK:
		return __syscall(__SYS_flock_get, fd, va_arg(ap, struct flock*));
	case F_SETLK:
		return __syscall(__SYS_flock_set, fd, va_arg(ap, struct flock*), 0);
	case F_SETLKW:
		return __syscall(__SYS_flock_set, fd, va_arg(ap, struct flock*), 1);
	default:
		errno = EINVAL;
		return -1;
	};
};
