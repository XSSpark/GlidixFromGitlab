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

#include <glidix/hw/idt.h>
#include <glidix/util/common.h>
#include <glidix/hw/port.h>
#include <glidix/hw/regs.h>
#include <glidix/util/string.h>
#include <glidix/util/log.h>
#include <glidix/util/panic.h>
#include <glidix/hw/apic.h>
#include <glidix/util/time.h>
#include <glidix/thread/sched.h>
#include <glidix/hw/cpu.h>
#include <glidix/hw/fpu.h>
#include <glidix/hw/idt.h>
#include <glidix/hw/pagetab.h>
#include <glidix/thread/process.h>

IDTEntry idt[256];
IDTPointer idtPtr;

static InterruptHandler intHandlers[256];
static void* intHandlerCtx[256];

extern void loadIDT();
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();
extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();
extern void isr48();
extern void isr49();
extern void isr50();
extern void isr51();
extern void isr52();
extern void isr53();
extern void isr54();
extern void isr55();
extern void isr56();
extern void isr57();
extern void isr58();
extern void isr59();
extern void isr60();
extern void isr61();
extern void isr62();
extern void isr63();
extern void isr64();
extern void isr65();
extern void isr112();
extern void isr113();
extern void isr114();
extern void irq_ditch();

static void setGate(int index, void *isr)
{
	uint64_t offset = (uint64_t) isr;
	idt[index].offsetLow = offset & 0xFFFF;
	idt[index].codeSegment = 8;
	idt[index].reservedIst = 0;
	idt[index].flags = 0x8E;		// present, DPL=0, type=interrupt gate
	idt[index].offsetMiddle = (offset >> 16) & 0xFFFF;
	idt[index].offsetHigh = (offset >> 32) & 0xFFFFFFFF;
	idt[index].reserved = 0;
};

static void setGateIST(int index, int ist)
{
	idt[index].reservedIst = ist;
};

void idtInit()
{
	// remap PIC interrups to the 0x80-0x8F range, so that we can ignore
	// them.
	outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x80);
	outb(0xA1, 0x88);
	outb(0x21, 4);
	outb(0xA1, 2);
	outb(0x21, 1);
	outb(0xA1, 1);
	
	// disable the legacy PIC
	outb(0xA1, 0xFF);
	outb(0x21, 0xFF);

	memset(idt, 0, 256*sizeof(IDTEntry));
	setGate(0, isr0);
	setGate(1, isr1);
	setGate(2, isr2);
	setGate(3, isr3);
	setGate(4, isr4);
	setGate(5, isr5);
	setGate(6, isr6);
	setGate(7, isr7);
	setGate(8, isr8);
	setGate(9, isr9);
	setGate(10, isr10);
	setGate(11, isr11);
	setGate(12, isr12);
	setGate(13, isr13);
	setGate(14, isr14);
	setGate(15, isr15);
	setGate(16, isr16);
	setGate(17, isr17);
	setGate(18, isr18);
	setGate(19, isr19);
	setGate(20, isr20);
	setGate(21, isr21);
	setGate(22, isr22);
	setGate(23, isr23);
	setGate(24, isr24);
	setGate(25, isr25);
	setGate(26, isr26);
	setGate(27, isr27);
	setGate(28, isr28);
	setGate(29, isr29);
	setGate(30, isr30);
	setGate(31, isr31);
	setGate(32, irq0);
	setGate(33, irq1);
	setGate(34, irq2);
	setGate(35, irq3);
	setGate(36, irq4);
	setGate(37, irq5);
	setGate(38, irq6);
	setGate(39, irq7);
	setGate(40, irq8);
	setGate(41, irq9);
	setGate(42, irq10);
	setGate(43, irq11);
	setGate(44, irq12);
	setGate(45, irq13);
	setGate(46, irq14);
	setGate(47, irq15);
	setGate(48, isr48);
	setGate(49, isr49);
	setGate(50, isr50);
	setGate(51, isr51);
	setGate(52, isr52);
	setGate(53, isr53);
	setGate(54, isr54);
	setGate(55, isr55);
	setGate(56, isr56);
	setGate(57, isr57);
	setGate(58, isr58);
	setGate(59, isr59);
	setGate(60, isr60);
	setGate(61, isr61);
	setGate(62, isr62);
	setGate(63, isr63);
	setGate(64, isr64);
	setGate(65, isr65);
	setGate(0x70, isr112);
	setGate(0x71, isr113);
	setGate(0x72, isr114);
	
	// set up IST for some
	setGateIST(I_NMI, 1);
	setGateIST(I_DOUBLE, 1);
	
	// PIC IRQs to be ignored
	int i;
	for (i=0x80; i<0x90; i++)
	{
		setGate(i, irq_ditch);
	};
	
	idtPtr.addr = (uint64_t) &idt[0];
	idtPtr.limit = (sizeof(IDTEntry) * 256) - 1;
	loadIDT();
};

noreturn void idtReboot()
{
	cli();
	idtPtr.addr = 0;
	idtPtr.limit = 0;
	loadIDT();
	ASM("int $0x70");
	while (1) ASM("cli; hlt");
};

static void isrDispatchSignal(Regs *regs, FPURegs *fpuRegs, ksiginfo_t *siginfo)
{
	kmcontext_gpr_t gprs;
	gprs.rax = regs->rax;
	gprs.rbx = regs->rbx;
	gprs.rcx = regs->rcx;
	gprs.rdx = regs->rdx;
	gprs.rsi = regs->rsi;
	gprs.rdi = regs->rdi;
	gprs.rbp = regs->rbp;
	gprs.rsp = regs->rsp;
	gprs.r8 = regs->r8;
	gprs.r9 = regs->r9;
	gprs.r10 = regs->r10;
	gprs.r11 = regs->r11;
	gprs.r12 = regs->r12;
	gprs.r13 = regs->r13;
	gprs.r14 = regs->r14;
	gprs.r15 = regs->r15;
	gprs.rip = regs->rip;
	gprs.rflags = regs->rflags;
	
	schedDispatchSignal(&gprs, fpuRegs, siginfo);
};

void isrHandler(Regs *regs, FPURegs *fpuregs)
{
	if (regs->intNo == I_PAGE_FAULT)
	{
		uint64_t faultAddr;
		ASM ("mov %%cr2, %%rax" : "=a" (faultAddr));

		if (((regs->cs & 3) == 0) || (regs->errCode & PF_RESERVED))
		{
			// the fault was triggered by code running in kernel mode, or by
			// reserved bits being invalid.
			panic("Page fault in kernel code "
				"(addr=0x%lx, rip=0x%lx, present=%d, write=%d, user=%d, reserved=%d, fetch=%d)",
				faultAddr, regs->rip,
				!!(regs->errCode & PF_PRESENT),
				!!(regs->errCode & PF_WRITE),
				!!(regs->errCode & PF_USER),
				!!(regs->errCode & PF_RESERVED),
				!!(regs->errCode & PF_FETCH)
			);
		};

		// valid page fault originating from userspace, we can enable interrupts and handle it
		sti();

		ksiginfo_t siginfo;
		if (procPageFault(faultAddr, regs->errCode, &siginfo) != 0)
		{
			isrDispatchSignal(regs, fpuregs, &siginfo);
		};
	}
	else if (regs->intNo == I_GPF)
	{
		panic("GPF occured (rip=0x%lx, code=0x%lx)\n", regs->rip, regs->errCode);
	}
	else if (regs->intNo == I_DOUBLE)
	{
		panic("The CPU double-faulted!");
	}
	else if (regs->intNo == I_APIC_TIMER)
	{
		apic.eoi = 0;
		__sync_synchronize();

		if (apic.timerCurrentCount == 0)
		{
			schedPreempt();
		};
	}
	else if (regs->intNo == I_IPI_WAKE)
	{
		apic.eoi = 0;
		__sync_synchronize();

		// if we are currently in the idle thread, we must switch task
		CPU *cpu = cpuGetCurrent();
		if (cpu->currentThread == &cpu->idleThread)
		{
			schedPreempt();
		};
	}
	else if (regs->intNo == I_IPI_MESSAGE)
	{
		apic.eoi = 0;
		__sync_synchronize();
		cpuProcessMessages();
	}
	else if (regs->intNo == IRQ0)
	{
		// the PIT is running at 1000 Hz
		timeIncrease(NANOS_PER_SEC/1000);
		apic.eoi = 0;
		__sync_synchronize();
	}
	else if (regs->intNo >= IRQ0 && regs->intNo <= IRQ15)
	{
		// miscellanous unhandled IRQs
		apic.eoi = 0;
		if (intHandlers[regs->intNo] != NULL)
		{
			intHandlers[regs->intNo](intHandlerCtx[regs->intNo]);
		};
		__sync_synchronize();
	}
	else
	{
		// unsupported interrupt
		panic("Received unexpected interrupt: %lu", regs->intNo);
	};

	// check for signals
	ksiginfo_t si;
	if (schedCheckSignals(&si) == 0)
	{
		isrDispatchSignal(regs, fpuregs, &si);
	};
};

void idtRegisterHandler(int intNo, InterruptHandler handler, void *ctx)
{
	if (intNo < 0 || intNo > 255)
	{
		panic("Invalid interrupt number passed to idtRegisterHandler: %d", intNo);
	};

	IrqState irqState = irqDisable();

	intHandlers[intNo] = handler;
	intHandlerCtx[intNo] = ctx;

	irqRestore(irqState);
};