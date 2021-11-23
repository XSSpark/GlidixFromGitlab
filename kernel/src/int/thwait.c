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

#include <glidix/int/thwait.h>
#include <glidix/hw/kom.h>
#include <glidix/thread/process.h>
#include <glidix/hw/pagetab.h>
#include <glidix/util/panic.h>

static int isConditionMet(uint64_t a, uint64_t b, int op)
{
	switch (op)
	{
	case THWAIT_EQUALS:				return a == b;
	case THWAIT_NEQUALS:				return a != b;
	default:					panic("invalid op, and went past EINVAL check!");
	};
};

errno_t sys_thwait(user_addr_t uptr, int op, uint64_t compare)
{
	if (op != THWAIT_EQUALS && op != THWAIT_NEQUALS)
	{
		return EINVAL;
	};

	if (uptr & 7)
	{
		return EINVAL;
	};

	char *page = (char*) procGetUserPage(uptr, PF_WRITE);
	if (page == NULL)
	{
		return EFAULT;
	};

	volatile uint64_t *valptr = (volatile uint64_t*) (page + (uptr & 0xFFF));

	KOM_UserPageInfo *info = komGetUserPageInfo(page);
	ASSERT(info != NULL);

	IrqState irqState = spinlockAcquire(&info->blockerLock);
	if (isConditionMet(*valptr, compare, op))
	{
		spinlockRelease(&info->blockerLock, irqState);
		return 0;
	};
	
	Blocker blocker;
	blocker.prev = NULL;
	blocker.next = (Blocker*) info->blockerList;
	if (blocker.next != NULL) blocker.next->prev = &blocker;
	blocker.offset = uptr & 0xFFF;
	blocker.waiter = schedGetCurrentThread();
	blocker.compareValue = compare;
	info->blockerList = &blocker;

	while (!isConditionMet(*valptr, compare, op) && !schedHaveReadySigs())
	{
		spinlockRelease(&info->blockerLock, irqState);
		schedSuspend();
		spinlockAcquire(&info->blockerLock);
	};

	if (blocker.prev != NULL) blocker.prev->next = blocker.next;
	if (blocker.next != NULL) blocker.next->prev = blocker.prev;
	if (info->blockerList == &blocker) info->blockerList = blocker.next;

	spinlockRelease(&info->blockerLock, irqState);

	komUserPageUnref(page);
	return 0;
};

errno_t sys_thsignal(user_addr_t uptr, uint64_t newValue)
{
	if (uptr & 7)
	{
		return EINVAL;
	};

	char *page = (char*) procGetUserPage(uptr, PF_WRITE);
	if (page == NULL)
	{
		return EFAULT;
	};

	KOM_UserPageInfo *info = komGetUserPageInfo(page);
	ASSERT(info != NULL);

	uint64_t offset = uptr & 0xFFF;
	IrqState irqState = spinlockAcquire(&info->blockerLock);

	Blocker *blocker;
	for (blocker=(Blocker*) info->blockerList; blocker!=NULL; blocker=blocker->next)
	{
		if (blocker->offset == offset && blocker->compareValue == newValue)
		{
			schedWake(blocker->waiter);
		};
	};

	spinlockRelease(&info->blockerLock, irqState);
	komUserPageUnref(page);
	return 0;
};