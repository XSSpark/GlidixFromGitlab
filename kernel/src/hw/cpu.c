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

#include <glidix/hw/cpu.h>
#include <glidix/hw/msr.h>
#include <glidix/hw/apic.h>
#include <glidix/hw/kom.h>
#include <glidix/hw/pagetab.h>
#include <glidix/util/panic.h>
#include <glidix/util/string.h>
#include <glidix/util/time.h>
#include <glidix/util/log.h>
#include <glidix/hw/idt.h>
#include <glidix/hw/fpu.h>

/**
 * In trampoline.asm: range of addresses where we have the real mode code for starting
 * up an AP.
 */
extern char _cpuTrampolineStart[];
extern char _cpuTrampolineEnd[];

static CPU cpuList[CPU_MAX];
static int nextCPUIndex = 1;

extern char GDT64;
extern struct
{
	uint16_t limit;
	uint64_t base;
} PACKED GDTPointer;
extern char idtPtr;

void cpuInitSelf(int index)
{
	CPU *me = &cpuList[index];
	me->self = me;
	me->apicID = apic.id >> 24;		// we need this for the initial CPU
	me->kernelCR3 = pagetabGetCR3();

	// set the GS segment to point to the CPU struct as expected
	wrmsr(MSR_GS_BASE, (uint64_t) me);

	// enable the local APIC at the default base address
	wrmsr(MSR_APIC_BASE, APIC_PHYS_BASE | APIC_BASE_ENABLE);

	// set the spurious interrupt vector
	apic.sivr = 0x1FF;

	// initialize the scheduler
	schedInitLocal();
};

void cpuRegister(uint8_t apicID)
{
	CPU *cpu = &cpuList[nextCPUIndex++];
	cpu->apicID = apicID;
};

static void cpuSendInterrupt(uint8_t apicID, uint32_t icr)
{
	apic.icrDestApicID = (uint32_t) apicID << 24;
	__sync_synchronize();
	apic.icr = icr;
	__sync_synchronize();
};

void cpuStartAPs()
{
	// create a pointer to low memory
	char *lowmem = (char*) komAllocVirtual(CPU_LOWMEM_SIZE);
	if (pagetabMapKernel(lowmem, 0, CPU_LOWMEM_SIZE, PT_WRITE | PT_NOEXEC) != 0)
	{
		panic("Failed to map lowmem!\n");
	};

	// load the trampoline code
	memcpy(lowmem + CPU_LOWMEM_TRAM_CODE, _cpuTrampolineStart, _cpuTrampolineEnd-_cpuTrampolineStart);

	// get a pointer to the data
	TrampolineData *tramData = (TrampolineData*) (lowmem + CPU_LOWMEM_TRAM_DATA);

	// get our PML4
	uint64_t *pml4BSP = (uint64_t*) 0xFFFFFFFFFFFFF000;

	// unmap the lowmem area that was mapped by the bootloader, as the tables are
	// given back as 'free memory', so will be corrupted
	pml4BSP[0] = 0;
	pagetabReload();

	// identity-map the lowmem area so that the trampoline code can run.
	// we will re-use the same PML4[0] for all cores (as they only need it during
	// startup), but keep it unmapped from our own space (to prevent NULL from pointing
	// there)
	if (pagetabMapKernel(NULL, 0, CPU_LOWMEM_SIZE, PT_WRITE) != 0)
	{
		panic("Failed to identity-map lowmem!");
	};

	// get the relevant PML4e then unmap it from our PML4
	uint64_t pml4EntZero = pml4BSP[0];
	pml4BSP[0] = 0;
	pagetabReload();

	// start each CPU
	int i;
	for (i=1; i<nextCPUIndex; i++)
	{
		CPU *cpu = &cpuList[i];
		kprintf("Starting CPU with APIC ID %hhu...\n", cpu->apicID);

		// allocate a new PML4 for this CPU:
		// (1) PML4[0] is set to the PML4e we got earlier, to identity-map lowmem
		// (2) PML4[510] is mapped to our own PML4[510], as this is where the kernel resides
		// (3) PML4[511] is mapped to itself (to its real version), for recursive mapping
		uint64_t *pml4 = (uint64_t*) komAllocBlock(KOM_BUCKET_PAGE, KOM_POOLBIT_ALL);
		if (pml4 == NULL)
		{
			panic("failed to allocate a PML4 for the AP!");
		};

		memset(pml4, 0, PAGE_SIZE);
		pml4[0] = pml4EntZero;
		pml4[510] = pml4BSP[510];
		pml4[511] = pagetabGetPhys(pml4) | PT_WRITE | PT_PRESENT | PT_NOEXEC;

		// load a copy of the PML4 into the page table area (so that the trampoline
		// can use it before it can address 64-bit memory).
		// we don't care about recursive mappinig working on this copy, so we don't
		// need to fix up pml4[511]
		memcpy(lowmem + CPU_LOWMEM_PML4, pml4, PAGE_SIZE);

		// copy the GDT to the temp location (GDT goes from `GDT64` to `GDTPointer`,
		// see bootstrap.asm).
		memcpy(lowmem + CPU_LOWMEM_GDT, &GDT64, (uint64_t) &GDTPointer - (uint64_t) &GDT64);

		// copy the GDT to the AP area
		memcpy(cpu->gdt, &GDT64, 64);
		cpu->gdtPtr.limit = 63;
		cpu->gdtPtr.base = cpu->gdt;

		// clear the 'continue flags' in the trampoline area
		tramData->flagAP2BSP = 0;
		tramData->flagBSP2AP = 0;
		tramData->flagAPDone = 0;

		// pass the true GDT pointer
		tramData->realGDTPtr = &cpu->gdtPtr;

		// set up the temp GDT pointer
		tramData->tempGDT.limit = GDTPointer.limit;
		tramData->tempGDT.base = CPU_LOWMEM_GDT;

		// pass the PML4
		tramData->pml4Phys = pagetabGetPhys(pml4);

		// pass the IDT
		tramData->idtPtrPtr = &idtPtr;

		// pass the stack
		tramData->initRSP = (uint64_t) cpu->startupStack + CPU_STARTUP_STACK_SIZE;

		// memory fence before we try to start the CPU
		__sync_synchronize();

		// initialize the CPU and to wait 10ms
		cpuSendInterrupt(cpu->apicID, APIC_ICR_DESTMODE_INIT | APIC_ICR_INITDEAS_NO);
		nanoseconds_t startTime = timeGetUptime();
		while (timeGetUptime() < startTime+10*NANOS_PER_SEC/1000);

		// try to launch the trampoline by sending SIPI
		cpuSendInterrupt(cpu->apicID, (CPU_LOWMEM_TRAM_CODE >> 12) | APIC_ICR_DESTMODE_SIPI | APIC_ICR_INITDEAS_NO);

		// wait for the core to respond
		startTime = timeGetUptime();
		while (timeGetUptime() < startTime+5*NANOS_PER_SEC/1000 && !tramData->flagAP2BSP);

		// try sending the SIPI again if the CPU didn't start
		if (!tramData->flagAP2BSP)
		{
			cpuSendInterrupt(cpu->apicID, (CPU_LOWMEM_TRAM_CODE >> 12) | APIC_ICR_DESTMODE_SIPI | APIC_ICR_INITDEAS_NO);
			startTime = timeGetUptime();
			while (timeGetUptime() < startTime+5*NANOS_PER_SEC/1000 && !tramData->flagAP2BSP);
		};

		// if it still didn't work, we have an issue
		if (!tramData->flagAP2BSP)
		{
			panic("AP failed to start!");
		};

		// tell the AP that it can continue
		tramData->flagBSP2AP = 1;

		// wait for the AP to complete initializing
		while (!tramData->flagAPDone);

		// report success
		kprintf("BSP: AP init done.\n");
	};
};

int cpuGetCount()
{
	return nextCPUIndex;
};

/**
 * This is called from trampoline.asm, once we've set up Long Mode and a full 64-bit
 * context. This function must never return!
 */
noreturn void _cpuApEntry()
{
	// tell the BSP that we are done with the trampoline data
	TrampolineData *tramData = (TrampolineData*) CPU_LOWMEM_TRAM_DATA;
	tramData->flagAPDone = 1;

	// Make sure to unmap PML4[0] where we've temporarily identity-mapped lowmem
	uint64_t *pml4 = (uint64_t*) 0xFFFFFFFFFFFFF000;
	pml4[0] = 0;
	pagetabReload();

	// init the FPU
	fpuInit();

	// perform per-CPU initialization
	int index;
	for (index=0; index<CPU_MAX; index++)
	{
		if (cpuList[index].apicID == apic.id >> 24)
		{
			break;
		};
	};

	kprintf("Performing per-CPU init on CPU %d (APIC ID %d)...\n", index, (int) (apic.id >> 24));
	cpuInitSelf(index);

	// now yield control to other threads
	sti();
	while (1)
	{
		schedSuspend();
	};
};

void cpuWake(int index)
{
	cpuSendInterrupt(cpuList[index].apicID, I_IPI_WAKE | APIC_ICR_INITDEAS_NO);
	while (apic.icr & APIC_ICR_PENDING) __sync_synchronize();
};

int cpuGetMyIndex()
{
	int i;
	for (i=0; i<CPU_MAX; i++)
	{
		if (cpuList[i].apicID == apic.id >> 24)
		{
			break;
		};
	};

	return i;
};

CPU* cpuGetIndex(int index)
{
	return &cpuList[index];
};