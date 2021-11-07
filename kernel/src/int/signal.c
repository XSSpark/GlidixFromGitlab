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

#include <glidix/int/signal.h>
#include <glidix/thread/process.h>
#include <glidix/util/log.h>

int sys_sigaction(int signum, user_addr_t uact, user_addr_t uoldact)
{
	SigAction sa1, sa2;

	SigAction *act = NULL;
	if (uact != 0) act = &sa1;

	SigAction *oldact = NULL;
	if (uoldact != 0) oldact = &sa2;

	if (act != NULL)
	{
		int status = procToKernelCopy(act, uact, sizeof(SigAction));
		if (status != 0) return status;
	};

	int status = schedSigAction(signum, act, oldact);
	if (status != 0) return status;

	if (oldact != NULL)
	{
		status = procToUserCopy(uoldact, oldact, sizeof(SigAction));
	};

	return status;
};

ksigset_t sys_sigmask(int how, ksigset_t mask)
{
	// can't affect SIGKILL, SIGSTOP, SIGTHKILL
	mask &= ~(1UL << SIGKILL);
	mask &= ~(1UL << SIGSTOP);
	mask &= ~(1UL << SIGTHKILL);

	Thread *me = schedGetCurrentThread();
	ksigset_t oldMask = me->sigBlocked;

	switch (how)
	{
	case SIG_BLOCK:
		me->sigBlocked |= mask;
		break;
	case SIG_UNBLOCK:
		me->sigBlocked &= ~mask;
		break;
	case SIG_SETMASK:
		me->sigBlocked = mask;
		break;
	};

	return oldMask;
};