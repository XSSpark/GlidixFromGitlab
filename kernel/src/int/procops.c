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

#include <glidix/int/procops.h>
#include <glidix/int/syscall.h>
#include <glidix/thread/process.h>
#include <glidix/util/memory.h>
#include <glidix/util/string.h>

/**
 * In fork.asm: takes the syscall context from the stack and enters the child userspace.
 */
noreturn void _forkEnterChild(SyscallContext *ctx);

static void forkEntry(void *context_)
{
	// now copy that context onto the stack and release the heap copy
	ALIGN(16) SyscallContext localContext;
	memcpy(&localContext, context_, sizeof(SyscallContext));
	kfree(context_);

	// now go to the child
	_forkEnterChild(&localContext);
};

pid_t sys_fork()
{
	// make a copy of the syscall context on the heap, so we can pass it to the child
	// startup
	SyscallContext *context = (SyscallContext*) kmalloc(sizeof(SyscallContext));
	if (context == NULL)
	{
		return -ENOMEM;
	};

	memcpy(context, schedGetCurrentThread()->syscallContext, sizeof(SyscallContext));

	// try to create the process, only release context if that doesn't work
	pid_t pid = procCreate(forkEntry, context);
	if (pid < 0)
	{
		kfree(context);
	};

	return pid;
};

pid_t sys_getpid()
{
	return schedGetCurrentThread()->proc->pid;
};

pid_t sys_getppid()
{
	return schedGetCurrentThread()->proc->parent;
};

pid_t sys_waitpid(pid_t pid, user_addr_t uwstatus, int flags)
{
	int wstatus;
	pid_t result = procWait(pid, &wstatus, flags);

	if (result > 0 && uwstatus != 0)
	{
		int status = procToUserCopy(uwstatus, &wstatus, sizeof(int));
		if (status != 0)
		{
			return status;
		};
	};

	return result;
};

int sys_setsid()
{
	return procSetSessionID();
};

pid_t sys_getsid()
{
	return schedGetCurrentThread()->proc->sid;
};

int sys_setpgid(pid_t pid, pid_t pgid)
{
	return procSetProcessGroup(pid, pgid);
};

pid_t sys_getpgrp()
{
	return schedGetCurrentThread()->proc->pgid;
};

int sys_kill(pid_t pid, int signo)
{
	return procKill(pid, signo);
};

thid_t sys_pthread_self()
{
	return schedGetCurrentThread()->thid;
};

int sys_raise(int signo)
{
	if (signo < 1 || signo >= SIG_NUM)
	{
		return -EINVAL;
	};

	ksiginfo_t si;
	memset(&si, 0, sizeof(ksiginfo_t));
	si.si_signo = signo;
	si.si_code = SI_USER;
	si.si_pid = schedGetCurrentThread()->proc->pid;
	si.si_uid = schedGetCurrentThread()->proc->ruid;

	sysDispatchSignal(&si, 0);
	return 0;
};

void sys_thexit(thretval_t retval)
{
	procExitThread(retval);
};

int sys_munmap(user_addr_t addr, size_t len)
{
	return procUnmap(addr, len);
};

int sys_mprotect(user_addr_t addr, size_t len, int prot)
{
	return procProtect(addr, len, prot);
};

errno_t sys_pthread_detach(thid_t thid)
{
	return procDetachThread(thid);
};