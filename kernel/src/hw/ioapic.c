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

#include <glidix/hw/ioapic.h>
#include <glidix/hw/kom.h>
#include <glidix/hw/pagetab.h>
#include <glidix/util/init.h>
#include <glidix/util/panic.h>
#include <glidix/util/string.h>
#include <glidix/util/log.h>
#include <glidix/util/memory.h>
#include <glidix/hw/idt.h>
#include <glidix/hw/apic.h>
#include <glidix/hw/cpu.h>

/**
 * Head of the I/O APIC list.
 */
static IOAPIC *ioapicHead;

/**
 * Head of the interrupt source override list.
 */
static InterruptOverride isaIntOvr[16];

/**
 * Lock controlling access to the IOAPIC regs.
 */
static Spinlock ioapicLock;

/**
 * Read an I/O APIC register.
 */
static uint32_t ioapicRead(IOAPIC *ioapic, uint32_t regno)
{
	ioapic->regs->regsel = regno;
	__sync_synchronize();
	uint32_t result = ioapic->regs->iowin;
	__sync_synchronize();

	return result;
};

/**
 * Write to an I/O APIC register.
 */
static void ioapicWrite(IOAPIC *ioapic, uint32_t regno, uint32_t value)
{
	ioapic->regs->regsel = regno;
	__sync_synchronize();
	ioapic->regs->iowin = value;
	__sync_synchronize();
};

/**
 * Process an I/O APIC record in the MADT.
 */
static void ioapicProcessIOAPIC(MADTRecord_IOAPIC *record)
{
	// map the register space
	IOAPICRegs *regs = (IOAPICRegs*) pagetabMapPhys(record->ioapicbase, sizeof(IOAPICRegs), PT_WRITE | PT_NOEXEC | PT_NOCACHE);
	if (regs == NULL)
	{
		panic("Failed to map the I/O APIC registers to memory!\n");
	};

	// create the IOAPIC instance
	IOAPIC *ioapic = (IOAPIC*) kmalloc(sizeof(IOAPIC));
	ioapic->id = record->id;
	ioapic->intbase = record->intbase;
	ioapic->regs = regs;
	ioapic->next = ioapicHead;
	ioapic->entcount = ((ioapicRead(ioapic, IOAPICVER) >> 16) & 0xFF) + 1;
	ioapicHead = ioapic;

	// done!
	kprintf("    Detected an I/O APIC (ioapicbase=0x%x, intbase=%u, entcount=%u)\n",
		record->ioapicbase, ioapic->intbase, ioapic->entcount);
};

/**
 * Process an "interrupt source override" record in the MADT.
 */
static void ioapicProcessIntOvr(MADTRecord_IntOvr *record)
{
	if (record->bus == 0 && record->irq < 16)
	{
		InterruptOverride *ovr = &isaIntOvr[record->irq];
		ovr->flags = record->flags;
		ovr->irq = record->irq;
		ovr->sysint = record->sysint;

		kprintf("    Detected an ISA interrupt mapping: ISA %hhu -> system %u (active %s, %s-triggered)\n",
			ovr->irq, ovr->sysint,
			ovr->flags & IOAPIC_INTFLAGS_LOW ? "low" : "high",
			ovr->flags & IOAPIC_INTFLAGS_LEVEL ? "level" : "edge");
	};
};

/**
 * Process a MADT LAPIC record.
 */
static void ioapicProcessLAPICRecord(MADTRecord_LAPIC *record)
{
	kprintf("    Found CPU with ID %hhu (%s)\n", record->id, record->flags & IOAPIC_LAPIC_ENABLED ? "enabled" : "disabled");
	if (record->flags & IOAPIC_LAPIC_ENABLED && record->id != (apic.id >> 24))
	{
		cpuRegister(record->id);
	};
};

/**
 * Process a record in the MADT table.
 */
static void ioapicProcessMADTRecord(MADTRecord *record)
{
	kprintf("  Processing MADT record type %hhu, length %hhu...\n", record->type, record->len);
	if (record->type == MADT_RECORD_IOAPIC)
	{
		ioapicProcessIOAPIC((MADTRecord_IOAPIC*) record->data);
	}
	else if (record->type == MADT_RECORD_INTOVR)
	{
		ioapicProcessIntOvr((MADTRecord_IntOvr*) record->data);
	}
	else if (record->type == MADT_RECORD_LAPIC)
	{
		ioapicProcessLAPICRecord((MADTRecord_LAPIC*) record->data);
	};
};

/**
 * Process the MADT (ACPI table with signature "APIC").
 */
static void ioapicProcessMADT(SDTHeader *header)
{
	char *scan = (char*) &header[1] + 8;			/* skip over the LAPIC addr and flags fields */
	char *end = (char*) header + header->len;

	while (scan < end)
	{
		MADTRecord *record = (MADTRecord*) scan;
		ioapicProcessMADTRecord(record);
		scan += record->len;
	};
};

/**
 * Process an ACPI table.
 */
static void ioapicProcessTable(SDTHeader *header)
{
	char sigbuf[5];
	memcpy(sigbuf, header->sig, 4);
	sigbuf[4] = 0;
	kprintf("Found ACPI table with signature [%4s] with size %u, processing...\n", sigbuf, header->len);

	if (memcmp(header->sig, "APIC", 4) == 0)
	{
		ioapicProcessMADT(header);
	};
};

/**
 * Return the I/O APIC corresponding to this system interrupt.
 */
static IOAPIC *ioapicGetForInt(uint32_t sysint)
{
	IOAPIC *ioapic;
	for (ioapic=ioapicHead; ioapic!=NULL; ioapic=ioapic->next)
	{
		if (ioapic->intbase <= sysint && ioapic->intbase+ioapic->entcount > sysint)
		{
			break;
		};
	};

	return ioapic;
};

void ioapicMap(int sysint, uint8_t vector, int polarity, int triggerMode)
{
	IrqState irqState = spinlockAcquire(&ioapicLock);

	IOAPIC *ioapic = ioapicGetForInt(sysint);
	if (ioapic == NULL)
	{
		panic("No I/O APIC for system interrupt %u!\n", sysint);
	};

	uint32_t intOffset = sysint - ioapic->intbase;

	IOAPICRedir redir;
	redir.lowerDword = redir.upperDword = 0;
	redir.vector = vector;
	redir.delvMode = IOAPIC_DELV_MODE_FIXED;
	redir.destMode = IOAPIC_DEST_MODE_PHYSICAL;
	redir.pinPolarity = polarity;
	redir.triggerMode = triggerMode;
	redir.destination = cpuGetIndex(0)->apicID;

	ioapicWrite(ioapic, IOAPICREDTBL(intOffset), redir.lowerDword);
	ioapicWrite(ioapic, IOAPICREDTBL(intOffset)+1, redir.upperDword);

	spinlockRelease(&ioapicLock, irqState);
};

void ioapicInit()
{
	if ((bootInfo->features & KB_FEATURE_RSDP) == 0)
	{
		panic("The bootloader did not pass an RSDP!");
	};

	uint32_t i;
	for (i=0; i<16; i++)
	{
		isaIntOvr[i].irq = i;
		isaIntOvr[i].sysint = i;
	};

	kprintf("RSDP physical address: 0x%lx\n", bootInfo->rsdpPhys);

	RSDPDescriptor *rsdp = pagetabMapPhys(bootInfo->rsdpPhys, sizeof(RSDPDescriptor), PT_WRITE | PT_NOEXEC);
	if (rsdp == NULL)
	{
		panic("Failed to map the RSDP!");
	};

	kprintf("RSDT physical address: 0x%x\n", rsdp->rsdtAddr);

	RSDT *rsdt = pagetabMapPhys(rsdp->rsdtAddr, sizeof(RSDT), PT_WRITE | PT_NOEXEC);
	if (rsdt == NULL)
	{
		panic("Failed to map the RSDT!");
	};

	uint32_t numAcpiTables = (rsdt->header.len - sizeof(SDTHeader)) / 4;
	if (numAcpiTables > MAX_ACPI_TABLES)
	{
		panic("Too many ACPI tables (%u)!", numAcpiTables);
	};

	kprintf("Found %u ACPI tables, processing...\n", numAcpiTables);

	for (i=0; i<numAcpiTables; i++)
	{
		uint32_t tablePhys = rsdt->acpiTables[i];
		SDTHeader *table = (SDTHeader*) pagetabMapPhys(tablePhys, PAGE_SIZE, PT_WRITE | PT_NOEXEC);
		if (table == NULL)
		{
			panic("Failed to map an ACPI table!");
		};

		if (table->len > PAGE_SIZE)
		{
			panic("ACPI table too large!");
		};

		ioapicProcessTable(table);
	};

	// map the interrupts
	for (i=0; i<16; i++)
	{
		if (isaIntOvr[i].sysint == i)
		{
			// mapped 1:1, check if another IRQ is using the sysint
			int j;
			for (j=0; j<16; j++)
			{
				if (i != j && isaIntOvr[j].sysint == i)
				{
					break;
				};
			};

			if (j != 16)
			{
				kprintf("ISA interrupt %d is unmapped.\n", i);
				continue;
			};
		};

		InterruptOverride *ovr = &isaIntOvr[i];
		kprintf("ISA interrupt %d is mapped to global interrupt %d (active %s, %s-triggered)\n",
			i, ovr->sysint,
			ovr->flags & IOAPIC_INTFLAGS_LOW ? "low" : "high",
			ovr->flags & IOAPIC_INTFLAGS_LEVEL ? "level" : "edge");

		IOAPIC *ioapic = ioapicGetForInt(ovr->sysint);
		if (ioapic == NULL)
		{
			panic("No I/O APIC for system interrupt %u!\n", ovr->sysint);
		};

		uint32_t intOffset = ovr->sysint - ioapic->intbase;

		IOAPICRedir redir;
		redir.lowerDword = redir.upperDword = 0;
		redir.vector = IRQ0 + i;
		redir.delvMode = IOAPIC_DELV_MODE_FIXED;
		redir.destMode = IOAPIC_DEST_MODE_PHYSICAL;
		if (ovr->flags & IOAPIC_INTFLAGS_LOW) redir.pinPolarity = IOAPIC_POLARITY_ACTIVE_LOW;
		if (ovr->flags & IOAPIC_INTFLAGS_LEVEL) redir.triggerMode = IOAPIC_TRIGGER_MODE_LEVEL;
		redir.destination = apic.id >> 24;

		ioapicWrite(ioapic, IOAPICREDTBL(intOffset), redir.lowerDword);
		ioapicWrite(ioapic, IOAPICREDTBL(intOffset)+1, redir.upperDword);
	};
};