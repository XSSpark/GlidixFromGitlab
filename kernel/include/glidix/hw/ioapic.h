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

#ifndef __glidix_hw_ioapic_h
#define	__glidix_hw_ioapic_h

#include <glidix/util/common.h>

/**
 * Maximum number of ACPI tables we can handle.
 */
#define	MAX_ACPI_TABLES				256

/**
 * MADT record types.
 */
#define	MADT_RECORD_LAPIC			0
#define	MADT_RECORD_IOAPIC			1
#define	MADT_RECORD_INTOVR			2

/**
 * I/O APIC registers.
 */
#define IOAPICID				0x00
#define IOAPICVER				0x01
#define IOAPICARB				0x02
#define IOAPICREDTBL(n)				(0x10 + 2 * n)

/**
 * Interrupt flags.
 */
#define	IOAPIC_INTFLAGS_LOW			(1 << 1)
#define	IOAPIC_INTFLAGS_LEVEL			(1 << 3)

/**
 * Delivery modes.
 */
#define	IOAPIC_DELV_MODE_FIXED			0

/**
 * Trigger modes.
 */
#define	IOAPIC_TRIGGER_MODE_EDGE		0
#define	IOAPIC_TRIGGER_MODE_LEVEL		1

/**
 * Destination modes.
 */
#define	IOAPIC_DEST_MODE_PHYSICAL		0
#define	IOAPIC_DEST_MODE_LOGICAL		1

/**
 * Pin polarity.
 */
#define	IOAPIC_POLARITY_ACTIVE_HIGH		0
#define	IOAPIC_POLAIRTY_ACTIVE_LOW		1

/**
 * LAPIC entry flags.
 */
#define	IOAPIC_LAPIC_ENABLED			(1 << 0)
#define	IOAPIC_LAPIC_ONLINE_CAPABLE		(1 << 1)

typedef union
{
	struct
	{
		uint64_t vector       : 8;
		uint64_t delvMode     : 3;
		uint64_t destMode     : 1;
		uint64_t delvStatus   : 1;
		uint64_t pinPolarity  : 1;
		uint64_t remoteIRR    : 1;
		uint64_t triggerMode  : 1;
		uint64_t mask         : 1;
		uint64_t reserved     : 39;
		uint64_t destination  : 8;
	};

	struct
	{
		uint32_t lowerDword;
		uint32_t upperDword;
	};
} IOAPICRedir;

/**
 * I/O APIC memory-mapped registers (REGSEL and IOWIN).
 */
typedef struct
{
	volatile uint32_t			regsel;		// 0x00
	volatile uint32_t			pad0;		// 0x04
	volatile uint32_t			pad1;		// 0x08
	volatile uint32_t			pad2;		// 0x0C
	volatile uint32_t			iowin;		// 0x10
} IOAPICRegs;

/**
 * Represents an I/O APIC.
 */
typedef struct IOAPIC_ IOAPIC;
struct IOAPIC_
{
	/**
	 * The next I/O APIC (in no particular order).
	 */
	IOAPIC *next;

	/**
	 * The I/O APIC registers.
	 */
	IOAPICRegs *regs;

	/**
	 * The I/O APIC ID.
	 */
	uint8_t id;

	/**
	 * Global interrupt base.
	 */
	uint32_t intbase;

	/**
	 * Number of interrupts handled by this I/O APIC.
	 */
	uint32_t entcount;
};

/**
 * Defines an interrupt source override.
 */
typedef struct
{
	/**
	 * The IRQ being overriden.
	 */
	uint8_t irq;

	/**
	 * The flags.
	 */
	uint16_t flags;

	/**
	 * The system interrupt number.
	 */
	uint32_t sysint;
} InterruptOverride;

/**
 * The RSDP descriptor.
 */
typedef struct
{
	char					sig[8];
	uint8_t					checksum;
	char					oemid[6];
	uint8_t					rev;
	uint32_t				rsdtAddr;

	// ACPI 2.0
	uint32_t				len;
	uint64_t				xsdtAddr;
	uint8_t					extChecksum;
	uint8_t					rsv[3];
} PACKED RSDPDescriptor;

typedef struct
{
	char					sig[4];
	uint32_t				len;
	uint8_t					rev;
	uint8_t					checksum;
	char					oemid[6];
	char					oemtabid[8];
	uint32_t				oemrev;
	uint32_t				crid;
	uint32_t				crev;
} PACKED SDTHeader;

typedef struct
{
	SDTHeader				header;
	uint32_t				acpiTables[MAX_ACPI_TABLES];
} PACKED RSDT;

typedef struct
{
	uint8_t					type;
	uint8_t					len;
	uint8_t					data[];
} PACKED MADTRecord;

typedef struct
{
	uint8_t					id;
	uint8_t					rsv;
	uint32_t				ioapicbase;
	uint32_t				intbase;
} PACKED MADTRecord_IOAPIC;

typedef struct
{
	uint8_t					bus;
	uint8_t					irq;
	uint32_t				sysint;
	uint16_t				flags;
} PACKED MADTRecord_IntOvr;

typedef struct
{
	uint8_t					acpiID;
	uint8_t					id;
	uint32_t				flags;
} PACKED MADTRecord_LAPIC;
/**
 * Initialize all the I/O APICs.
 */
void ioapicInit();

#endif