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

#ifndef __glidix_thread_signal_h
#define	__glidix_thread_signal_h

#include <glidix/util/common.h>

#ifndef SIGHUP
#define	SIGHUP						1
#define	SIGINT						2
#define	SIGQUIT						3
#define	SIGILL						4
#define	SIGTRAP						5
#define	SIGABRT						6
#define	SIGEMT						7
#define	SIGFPE						8
#define	SIGKILL						9
#define	SIGBUS						10
#define	SIGSEGV						11
#define	SIGSYS						12
#define	SIGPIPE						13
#define	SIGALRM						14
#define	SIGTERM						15
#define	SIGUSR1						16
#define	SIGUSR2						17
#define	SIGCHLD						18
#define	SIGPWR						19
#define	SIGWINCH					20
#define	SIGURG						21
#define	SIGPOLL						22
#define	SIGSTOP						23
#define	SIGTSTP						24
#define	SIGCONT						25
#define	SIGTTIN						26
#define	SIGTTOU						27
#define	SIGVTALRM					28
#define	SIGPROF						29
#define	SIGXCPU						30
#define	SIGXFSZ						31
#define	SIGWAITING					32
#define	SIGLWP						33
#define	SIGAIO						34
#define	SIGTHKILL					35		/* kill a single thread */
#define	SIGTHWAKE					36		/* wake a thread without dispatching a signal */
#define	SIGTRACE					37		/* debugger notification */
#define	SIGTHSUSP					38		/* suspend thread */

/**
 * si_code for SIGSEGV
 */
#define	SEGV_MAPERR					0x1001
#define	SEGV_ACCERR					0x1002

/**
 * si_code for SIGBUS
 */
#define	BUS_ADRALN					0x4001
#define	BUS_ADRERR					0x4002
#define	BUS_OBJERR					0x4003

/**
 * sigaction sa_flags
 */
#define	SA_NOCLDSTOP					(1 << 0)
#define	SA_NOCLDWAIT					(1 << 1)
#define	SA_NODEFER					(1 << 2)
#define	SA_ONSTACK					(1 << 3)
#define	SA_RESETHAND					(1 << 4)
#define	SA_RESTART					(1 << 5)
#define	SA_SIGINFO					(1 << 6)

#endif		/* SIGHUP */
#define	SIG_NUM						39

/**
 * Signal value (either an integer or a pointer, depending on the signal type).
 */
union ksigval
{
	/**
	 * The integer value.
	 */
	int sival_int;

	/**
	 * The pointer value.
	 */
	void* sival_ptr;
};

/**
 * Represents information about a signal.
 */
typedef struct
{
	/**
	 * The signal number.
	 */
	int si_signo;

	/**
	 * Signal code, essentially a subtype of signal. Meaning depends on the
	 * type (`si_signo`).
	 */
	int si_code;

	/**
	 * Error code associated with the signal.
	 */
	int si_errno;

	/**
	 * Process ID of the sending process.
	 */
	pid_t si_pid;

	/**
	 * Real user ID of the sending process.
	 */
	uid_t si_uid;

	/**
	 * Address related to the signal (e.g. failing instruction address, or memory
	 * address referenced for `SIGSEGV`, etc).
	 */
	void* si_addr;

	/**
	 * Exit value or signal for process termination.
	 */
	int si_status;

	/**
	 * Band event for SIGPOLL/SIGIO.
	 */
	long si_band;

	/**
	 * Signal value.
	 */
	union ksigval si_value;
} ksiginfo_t;

/**
 * Must match 'struct sigaction' from libc.
 */
typedef struct
{
	uint64_t sa_sigaction_handler;
	uint64_t sa_mask;
	int sa_flags;
} SigAction;

/**
 * Set of signals.
 */
typedef uint64_t ksigset_t;

/**
 * System call to change the disposition of a signal.
 */
int sys_sigaction(int signum, uint64_t act, uint64_t oldact);

#endif		/* __glidix_thread_signal_h */