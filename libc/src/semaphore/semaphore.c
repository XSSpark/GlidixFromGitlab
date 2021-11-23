/*
	Glidix Standard C Library (libc)
	
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

#include <sys/gxthread.h>
#include <semaphore.h>
#include <errno.h>

int sem_init(sem_t *sem, int pshared, unsigned value)
{
	// all Glidix semaphores are available for sharing
	(void) pshared;
	sem->__value = value;
	return 0;
};

int sem_destroy(sem_t *sem)
{
	return 0;
};

int sem_wait(sem_t *sem)
{
	while (1)
	{
		int64_t currentValue = sem->__value;
		if (currentValue == 0)
		{
			thwait(&sem->__value, THWAIT_NEQUALS, 0);
			continue;
		};

		if (__sync_val_compare_and_swap(&sem->__value, currentValue, currentValue-1) != currentValue)
		{
			continue;
		};

		return 0;
	};
};

int sem_post(sem_t *sem)
{
	if (__sync_fetch_and_add(&sem->__value, 1) == 0)
	{
		// wake up potentially waiting threads
		thsignal(&sem->__value, 0);
	};

	return 0;
};

int sem_getvalue(sem_t *sem, int *valptr)
{
	*valptr = (int) sem->__value;
	return 0;
};
