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

#include <glidix/util/init.h>
#include <glidix/hw/idt.h>
#include <glidix/display/console.h>
#include <glidix/util/log.h>
#include <glidix/hw/kom.h>
#include <glidix/hw/cpu.h>
#include <glidix/hw/apic.h>
#include <glidix/hw/fpu.h>
#include <glidix/hw/port.h>
#include <glidix/thread/sched.h>
#include <glidix/hw/ioapic.h>
#include <glidix/util/time.h>
#include <glidix/util/string.h>
#include <glidix/util/panic.h>

/**
 * The terminator of the kernel init action list, see `kernel.ld` for an
 * explanation.
 */
SECTION(".kia_terminator") KernelInitAction __kia_terminator = {.initFunc = NULL};

KernelBootInfo *bootInfo;

/**
 * Run a kernel init action with the specified name.
 */
static void kiaRun(const char *name)
{
	KernelInitAction *kia;
	for (kia=kiaList; kia->initFunc!=NULL; kia++)
	{
		if (strcmp(kia->links[0], name) == 0)
		{
			break;
		};
	};

	if (kia->initFunc == NULL)
	{
		panic("Failed to find kernel init action named `%s'", name);
	};

	if (kia->complete)
	{
		// already done
		return;
	};

	if (kia->started)
	{
		panic("Dependency loop in kernel init actions!");
	};

	// announce that we've started, to detect dependency loops
	kia->started = 1;

	// run the dependent actions
	int i;
	for (i=1; kia->links[i]!=NULL; i++)
	{
		kiaRun(kia->links[i]);
	};

	// now announce and run this one
	kprintf("Running kernel init action `%s'...\n", name);
	kia->initFunc();
};

void kmain(KernelBootInfo *info)
{
	// let other code access the boot information;
	// this needs to be done before ANYTHING else!
	bootInfo = info;

	// initialize the console
	conInit();
	kprintf("Glidix kernel, version %s\n", KERNEL_VERSION);
	kprintf("Copyright (c) 2021, Madd Games.\n");
	kprintf("All rights reserved.\n\n");

	// initialize the FPU
	kprintf("Initializing the FPU...\n");
	fpuInit();
	
	// initialize the IDT
	kprintf("Initializing the IDT...\n");
	idtInit();

	// initialize the kernel object manager
	kprintf("Initializing the Kernel Object Manager (KOM)...\n");
	komInit();

	// re-map the framebuffer
	kprintf("Remapping the console framebuffer...\n");
	conRemapFramebuffers();

	// initialize the scheduler globally
	kprintf("Initializing scheduler globally...\n");
	schedInitGlobal();
	
	// initialize this CPU
	kprintf("Initializing bootstrap CPU structures...\n");
	cpuInitSelf(0);

	// initialize the I/O APICs and CPUs
	kprintf("Initializing the I/O APICs...\n");
	ioapicInit();

	// initialize the PIT
	kprintf("Initializing the PIT...\n");
	uint16_t divisor = 1193180 / 1000;		// 1000 Hz
	outb(0x43, 0x36);
	uint8_t l = (uint8_t)(divisor & 0xFF);
	uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );
	outb(0x40, l);
	outb(0x40, h);

	// we can enable interrupts now
	ASM ("sti");

	// initialize the scheduling timer
	kprintf("Initializing the APIC timer for scheduling...\n");
	schedInitTimer();

	// find out the number of CPUs
	kprintf("Found %d CPUs, starting up AP cores...\n", cpuGetCount());
	cpuStartAPs();

	// run the kernel init actions
	kprintf("Running kernel init actions...\n");
	KernelInitAction *kia;
	for (kia=kiaList; kia->initFunc!=NULL; kia++)
	{
		kiaRun(kia->links[0]);
	};

	// now yield to other threads
	while (1)
	{
		schedSuspend();
	};
};