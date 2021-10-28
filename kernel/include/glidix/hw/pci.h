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

#ifndef __glidix_hw_pci_h
#define	__glidix_hw_pci_h

#include <glidix/util/common.h>

/**
 * PCI device enumeration init action name.
 */
#define	KIA_PCI_ENUM_DEVICES				"pciEnumDevices"

/**
 * The PCI config address port number.
 */
#define	PCI_CONFIG_ADDR					0xCF8

/**
 * The PCI config data port number.
 */
#define	PCI_CONFIG_DATA					0xCFC

#define	PCI_HEADER_TYPE_MASK				0x7F
#define	PCI_HEADER_TYPE_MULTIFUNC			0x80

#define	PCI_HEADER_TYPE_NORMAL				0
#define	PCI_HEADER_TYPE_PCI_BRIDGE			1

#define	PCI_VENDOR_NULL					0xFFFF

#define	PCI_REG_ADDR(bus, slot, func, reg)		(bus << 16) | (slot << 11) | (func << 8) | (1 << 31) | (reg)

#define	PCI_REG_BAR(n)					(0x10+4*n)

/**
 * The PCI configuration space with various header types.
 */
typedef union
{
	struct
	{
		uint16_t				vendor;
		uint16_t				device;
		uint16_t				command;
		uint16_t				status;
		uint8_t					rev;
		uint8_t					progif;
		uint8_t					subclass;
		uint8_t					classcode;
		uint8_t					cacheLineSize;
		uint8_t					latencyTimer;
		uint8_t					headerType;
		uint8_t					bist;
		uint32_t				bar[6];
		uint32_t				cardbusCIS;
		uint16_t				subsysVendor;
		uint16_t				subsysID;
		uint32_t				expromBase;
		uint8_t					cap;
		uint8_t					resv[7];
		uint8_t					intline;
		uint8_t					intpin;
		uint8_t					mingrant;
		uint8_t					maxlat;
	} std;
	
	struct
	{
		uint16_t				vendor;
		uint16_t				device;
		uint16_t				command;
		uint16_t				status;
		uint8_t					rev;
		uint8_t					progif;
		uint8_t					subclass;
		uint8_t					classcode;
		uint8_t					cacheLineSize;
		uint8_t					latencyTimer;
		uint8_t					headerType;
		uint8_t					bist;
		uint32_t				bar[2];
		uint8_t					primaryBus;
		uint8_t					secondaryBus;
		uint8_t					subordinateBus;
		uint8_t					secondaryLatencyTimer;
		uint8_t					iobase;
		uint8_t					iolimit;
		uint16_t				secondaryStatus;
		uint16_t				membase;
		uint16_t				memlimit;
		uint16_t				premembase;
		uint16_t				prememlimit;
		uint32_t				premembaseupper;
		uint32_t				prememlimitupper;
		uint16_t				iobaseupper;
		uint16_t				iolimitupper;
		uint8_t					capability;
		uint8_t					reserved[7];
		uint32_t				expbase;
		uint8_t					intline;
		uint8_t					intpin;
		uint16_t				bridgectl;
	} bridge;

	uint32_t words[16];
} PCIDeviceConfig;

/**
 * Represents a mapping in the PCI IRQ routing table.
 */
typedef struct PCIIntRouting_ PCIIntRouting;
struct PCIIntRouting_
{
	/**
	 * Link.
	 */
	PCIIntRouting *next;

	/**
	 * The slot number on the primary bus.
	 */
	uint8_t slot;

	/**
	 * The interrupt pin.
	 */
	uint8_t intpin;

	/**
	 * The global system interrupt (or -1 if this is an IRQ).
	 */
	int gsi;

	/**
	 * The interrupt vector we've mapped to.
	 */
	uint8_t vector;
};

/**
 * Represents a bridge.
 */
typedef struct PCIBridge_
{
	struct PCIBridge_*			up;
	uint8_t					masterSlot;
} PCIBridge;

/**
 * Represents a PCI BAR.
 */
typedef struct
{
	/**
	 * If this is not NULL, then the virtual memory address to which we were mapped.
	 */
	void *memAddr;

	/**
	 * If this is nonzero, then the base port.
	 */
	uint16_t basePort;

	/**
	 * Size of the BAR.
	 */
	uint32_t barsz;
} PCI_BAR;

/**
 * Represents a PCI device.
 */
typedef struct PCIDevice_ PCIDevice;
struct PCIDevice_
{
	/**
	 * Link.
	 */
	PCIDevice *next;

	/**
	 * Device address.
	 */
	uint8_t bus;
	uint8_t slot;
	uint8_t func;

	/**
	 * The BARs.
	 */
	PCI_BAR bars[6];
};

/**
 * Read from an address in the PCI configuration space. `addr` must be aligned on
 * a 4-byte boundary.
 */
uint32_t pciReadConfigReg(uint32_t addr);

/**
 * Write to an address in the PCI configuration space. `addr` must be aligned on
 * a 4-byte boundary.
 */
void pciWriteConfigReg(uint32_t addr, uint32_t value);

#endif