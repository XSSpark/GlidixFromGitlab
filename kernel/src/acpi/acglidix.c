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

#include <acpi/acpi.h>
#include <glidix/util/memory.h>
#include <glidix/display/console.h>
#include <glidix/util/time.h>
#include <glidix/hw/port.h>
#include <glidix/util/string.h>
#include <glidix/thread/sched.h>
#include <glidix/util/common.h>
#include <glidix/hw/pci.h>
#include <glidix/hw/pagetab.h>
#include <glidix/hw/idt.h>
#include <glidix/thread/mutex.h>
#include <glidix/util/init.h>
#include <glidix/util/panic.h>
#include <glidix/thread/spinlock.h>
#include <glidix/util/log.h>

/**
 * Implements the ACPICA Operating System Layer (OSL) functions.
 */

//#define	TRACE()			kprintf("[ACGLIDIX] %s\n", __func__);
#define	TRACE()

/**
 * Single page, used for reading/writing physical memory.
 */
static ALIGN(PAGE_SIZE) char acpiTransferPage[PAGE_SIZE];

/**
 * Lock for the transfer page.
 */
static Mutex acpiTransferLock;

void* AcpiOsAllocate(ACPI_SIZE size)
{
	TRACE();
	return kmalloc(size);
};

void AcpiOsFree(void *ptr)
{
	TRACE();
	kfree(ptr);
};

void AcpiOsVprintf(const char *fmt, va_list ap)
{
	kvprintf(fmt, ap);
};

void AcpiOsPrintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	kvprintf(fmt, ap);
	va_end(ap);
};

void AcpiOsSleep(UINT64 ms)
{
	TRACE();
	timeSleep(TIME_MILLI(ms));
};

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer()
{
	TRACE();
	return bootInfo->rsdpPhys;
};

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS port, UINT32 value, UINT32 width)
{
	TRACE();
	switch (width)
	{
	case 8:
		outb(port, (uint8_t) value);
		return AE_OK;
	case 16:
		outw(port, (uint16_t) value);
		return AE_OK;
	case 32:
		outd(port, (uint32_t) value);
		return AE_OK;
	default:
		return AE_ERROR;
	};
};

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS port, UINT32 *value, UINT32 width)
{
	TRACE();
	*value = 0;
	switch (width)
	{
	case 8:
		*((uint8_t*)value) = inb(port);
		return AE_OK;
	case 16:
		*((uint16_t*)value) = inw(port);
		return AE_OK;
	case 32:
		*((uint32_t*)value) = ind(port);
		return AE_OK;
	default:
		return AE_ERROR;
	};
};

void AcpiOsStall(UINT32 ms)
{
	// microseconds for some reason
	TRACE();
	int then = timeGetUptime() + (ms/1000 + 1);
	while (timeGetUptime() < then);
};

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS addr, UINT64 *value, UINT32 width)
{
	TRACE();
	*value = 0;

	mutexLock(&acpiTransferLock);
	if (pagetabMapKernel(acpiTransferPage, addr & ~0xFFFUL, PAGE_SIZE, PT_WRITE | PT_NOEXEC | PT_NOCACHE) != 0)
	{
		panic("Failed to map the transfer page for AcpiOsReadMemory()!");
	};

	memcpy(value, acpiTransferPage + (addr & 0xFFF), width/8);
	mutexUnlock(&acpiTransferLock);

	return AE_OK;
};

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS addr, UINT64 value, UINT32 width)
{
	TRACE();

	mutexLock(&acpiTransferLock);
	if (pagetabMapKernel(acpiTransferPage, addr & ~0xFFFUL, PAGE_SIZE, PT_WRITE | PT_NOEXEC | PT_NOCACHE) != 0)
	{
		panic("Failed to map the transfer page for AcpiOsReadMemory()!");
	};

	memcpy(acpiTransferPage + (addr & 0xFFF), &value, width/8);
	mutexUnlock(&acpiTransferLock);

	return AE_OK;
};

void* AcpiOsAllocateZeroed(ACPI_SIZE size)
{
	TRACE();
	void *ret = kmalloc(size);
	memset(ret, 0, size);
	return ret;
};

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *spinlock)
{
	TRACE();
	/**
	 * And the kmalloc() will return a block of 16 bytes to store a single spinlock (one byte). It will
	 * also fragment the kernel heap. Perhaps we should have a function for such "microallocations"?
	 */
	*spinlock = (Spinlock*) kmalloc(sizeof(Spinlock));
	spinlockInit(*spinlock);
	return AE_OK;
};

void AcpiOsDeleteLock(ACPI_SPINLOCK spinlock)
{
	TRACE();
	kfree(spinlock);
};

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK spinlock)
{
	TRACE();
	return spinlockAcquire(spinlock);
};

void AcpiOsReleaseLock(ACPI_SPINLOCK spinlock, ACPI_CPU_FLAGS flags)
{
	TRACE();
	spinlockRelease(spinlock, flags);
};

BOOLEAN AcpiOsReadable(void *mem, ACPI_SIZE size)
{
	TRACE();
	// this should not actually be called
	return 0;
};

ACPI_THREAD_ID AcpiOsGetThreadId()
{
	TRACE();
	return (ACPI_THREAD_ID) schedGetCurrentThread();
};

UINT64 AcpiOsGetTimer()
{
	TRACE();
	return timeGetUptime() / 1000;		// microseconds
};

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 maxUnits, UINT32 initUnits, ACPI_SEMAPHORE *semptr)
{
	TRACE();
	(void)maxUnits;
	if (semptr == NULL)
	{
		return AE_BAD_PARAMETER;
	};
	
	*semptr = (ACPI_SEMAPHORE) kmalloc(sizeof(Semaphore));
	semInit2(*semptr, initUnits);
	return AE_OK;
};

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE sem)
{
	TRACE();
	if (sem == NULL) return AE_BAD_PARAMETER;
	kfree(sem);
	return AE_OK;
};

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE sem, UINT32 units, UINT16 timeout)
{
	TRACE();
	int count = (int) units;
	if (sem == NULL)
	{
		return AE_BAD_PARAMETER;
	};
	
	if (units == 0)
	{
		return AE_OK;
	};

	int flags;
	uint64_t nanoTimeout;
	
	if (timeout == 0)
	{
		flags = SEM_W_NONBLOCK;
		nanoTimeout = 0;
	}
	else if (timeout == 0xFFFF)
	{
		flags = 0;
		nanoTimeout = 0;
	}
	else
	{
		flags = 0;
		nanoTimeout = (uint64_t) timeout * 1000000;
	};
	
	uint64_t deadline = timeGetUptime() + nanoTimeout;
	int acquiredSoFar = 0;
	
	while (count > 0)
	{
		int got = semWaitGen(sem, count, flags, nanoTimeout);
		if (got < 0)
		{
			semSignal2(sem, acquiredSoFar);
			return AE_TIME;
		};
		
		acquiredSoFar += got;
		count -= got;
		
		uint64_t now = timeGetUptime();
		if (now < deadline)
		{
			nanoTimeout = deadline - now;
		}
		else if (nanoTimeout != 0)
		{
			semSignal2(sem, acquiredSoFar);
			return AE_TIME;
		};
	};
	
	return AE_OK;
};

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE sem, UINT32 units)
{
	TRACE();
	if (sem == NULL)
	{
		return AE_BAD_PARAMETER;
	};
	
	if (units == 0)
	{
		return AE_OK;
	};
	
	semSignal2(sem, (int) units);
	return AE_OK;
};

ACPI_STATUS AcpiOsGetLine(char *buffer, UINT32 len, UINT32 *read)
{
	TRACE();
	*read = 0;
	return AE_OK;
};

ACPI_STATUS AcpiOsSignal(UINT32 func, void *info)
{
	TRACE();
	if (func == ACPI_SIGNAL_FATAL)
	{
		ACPI_SIGNAL_FATAL_INFO *fin = (ACPI_SIGNAL_FATAL_INFO*) info;
		panic("ACPI fatal: type %d, code %d, arg %d", fin->Type, fin->Code, fin->Argument);
	}
	else
	{
		kprintf("ACPI breakpoint: %s\n", (char*)info);
	};
	
	return AE_OK;
};

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE type, ACPI_OSD_EXEC_CALLBACK func, void *ctx)
{
	TRACE();
	// coincidently, ACPICA uses the same type of callback function as CreateKernelThread
	(void)type;
	if (func == NULL) return AE_BAD_PARAMETER;
	Thread *thread = schedCreateKernelThread(func, ctx, NULL);
	if (thread == NULL)
	{
		return AE_STACK_OVERFLOW;
	};

	schedDetachKernelThread(thread);
	return AE_OK;
};

ACPI_STATUS AcpiOsInitialize()
{
	TRACE();
	return AE_OK;
};

ACPI_STATUS AcpiOsTerminate()
{
	TRACE();
	return AE_OK;
};

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue)
{
	TRACE();
	*NewValue = NULL;
	return AE_OK;
};

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable)
{
	TRACE();
	*NewTable = NULL;
	return AE_OK;
};

void AcpiOsWaitEventsComplete()
{
	TRACE();
};

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength)
{
	TRACE();
	*NewAddress = 0;
	return AE_OK;
};

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *id, UINT32 reg, UINT64 *value, UINT32 width)
{
	TRACE();
	uint32_t regAligned = reg & ~3;
	uint32_t offsetIntoReg = reg & 3;
	uint32_t addr = (id->Bus << 16) | (id->Device << 11) | (id->Function << 8) | (1 << 31) | regAligned;
	
	union
	{
		uint32_t regval;
		char bytes[4];
	} regu;
	regu.regval = pciReadConfigReg(addr);
	
	char *fieldptr = regu.bytes + offsetIntoReg;
	size_t count = width/8;	
	*value = 0;
	memcpy(value, fieldptr, count);
	return AE_OK;
};

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *id, UINT32 reg, UINT64 value, UINT32 width)
{
	TRACE();
	uint32_t regAligned = reg & ~3;
	uint32_t offsetIntoReg = reg & 3;
	uint32_t addr = (id->Bus << 16) | (id->Device << 11) | (id->Function << 8) | (1 << 31) | regAligned;
	
	union
	{
		uint32_t regval;
		char bytes[4];
	} regu;

	regu.regval = pciReadConfigReg(addr);
	
	char *fieldptr = regu.bytes + offsetIntoReg;
	size_t count = width/8;
	memcpy(fieldptr, &value, count);
	
	pciWriteConfigReg(addr, regu.regval);
	
	return AE_OK;
};

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 irq, ACPI_OSD_HANDLER handler, void *context)
{
	TRACE();
	idtRegisterHandler(IRQ0+irq, (InterruptHandler) handler, context);
	return AE_OK;
};

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 intno, ACPI_OSD_HANDLER handler)
{
	TRACE();
	(void)intno;
	(void)handler;
	return AE_OK;
};

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS phaddr, ACPI_SIZE len)
{
	TRACE();
	return pagetabMapPhys(phaddr, len, PT_WRITE | PT_NOEXEC);
};

void AcpiOsUnmapMemory(void *laddr, ACPI_SIZE len)
{
	TRACE();
	// TODO
};

ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX *out)
{
	Mutex *mutex = (Mutex*) kmalloc(sizeof(Mutex));
	mutexInit(mutex);
	*out = mutex;
	return AE_OK;
};

void AcpiOsDeleteMutex(ACPI_MUTEX mutex)
{
	kfree(mutex);
};

ACPI_STATUS AcpiOsAcquireMutex(ACPI_MUTEX handle, UINT16 timeout)
{
	// TODO: take timeout into account
	mutexLock((Mutex*)handle);
	return AE_OK;
};

void AcpiOsReleaseMutex(ACPI_MUTEX handle)
{
	mutexUnlock((Mutex*)handle);
};

// --- SUPPORT FUNCTIONS ACPICA NEEDS ---
int strncmp(const char *s1, const char *s2, size_t n)
{
	for ( ; n > 0; s1++, s2++, --n)
		if (*s1 != *s2)
			return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
		else if (*s1 == '\0')
			return 0;
	return 0;
};

int isprint(int c)
{
	return ((c >= ' ' && c <= '~') ? 1 : 0);
};

char* strncpy(char *s1, const char *s2, size_t n)
{
	char *s = s1;
	while (n > 0 && *s2 != '\0') {
		*s++ = *s2++;
		--n;
	}
	while (n > 0) {
		*s++ = '\0';
		--n;
	}
	return s1;
};

int isdigit(int c)
{
	return((c>='0') && (c<='9'));
};

int isspace(int c)
{
	return (c == ' ' || c == '\t' || c == '\n' || c == '\12');
};

int isxdigit(int c)
{
	return isdigit(c) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F'));
};

int isupper(int c)
{
	return (c >= 'A') && (c <= 'Z');
};

int islower(int c)
{
	return (c >= 'a') && (c <= 'z');
};

int toupper(int c)
{
	if ((c >= 'a') && (c <= 'z'))
	{
		return c-'a'+'A';
	};
	
	return c;
};

int tolower(int c)
{
	if ((c >= 'A') && (c <= 'Z'))
	{
		return c-'A'+'a';
	};
	
	return c;
};

char * strncat(char *dst, const char *src, size_t n)
{
	if (n != 0) {
		char *d = dst;
		register const char *s = src;

		while (*d != 0)
			d++;
		do {
			if ((*d = *s++) == 0)
				break;
			d++;
		} while (--n != 0);
		*d = 0;
	}
	return (dst);
};

int isalpha(int c)
{
	return((c >='a' && c <='z') || (c >='A' && c <='Z'));
};

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
	register const char *s = nptr;
	register unsigned long acc;
	register int c;
	register unsigned long cutoff;
	register int neg = 0, any, cutlim;

	/*
	 * See strtol for comments as to the logic used.
	 */
	do {
		c = *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else if (c == '+')
		c = *s++;
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	} else if ((base == 0 || base == 2) &&
	    c == '0' && (*s == 'b' || *s == 'B')) {
		c = s[1];
		s += 2;
		base = 2;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;
	cutoff = (unsigned long)ULONG_MAX / (unsigned long)base;
	cutlim = (unsigned long)ULONG_MAX % (unsigned long)base;
	for (acc = 0, any = 0;; c = *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0) {
		acc = ULONG_MAX;
	} else if (neg)
		acc = -acc;
	if (endptr != 0)
		*endptr = (char *)(any ? s - 1 : nptr);
	return (acc);
};

char *strstr(const char *in, const char *str)
{
    char c;
    size_t len;

    c = *str++;
    if (!c)
        return (char *) in;	// Trivial empty string case

    len = strlen(str);
    do {
        char sc;

        do {
            sc = *in++;
            if (!sc)
                return (char *) 0;
        } while (sc != c);
    } while (strncmp(in, str, len) != 0);

    return (char *) (in - 1);
};