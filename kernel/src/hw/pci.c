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

#include <glidix/hw/pci.h>
#include <glidix/hw/port.h>
#include <glidix/thread/mutex.h>
#include <glidix/util/init.h>
#include <glidix/hw/acpi.h>
#include <glidix/util/log.h>
#include <glidix/util/panic.h>
#include <glidix/util/memory.h>
#include <glidix/hw/idt.h>
#include <glidix/hw/ioapic.h>
#include <glidix/hw/pagetab.h>

/**
 * PCI interrupt routing table.
 */
static PCIIntRouting* pciIntRoutingTable;

/**
 * List of PCI devices.
 */
static PCIDevice *pciDeviceHead;

/**
 * Next interrupt number to assign to a statically-mapped IRQ.
 */
static uint8_t pciNextIntVector = I_PCI0;

/**
 * The lock which controls access to PCI shared structures and config space.
 */
static Mutex pciLock;

/**
 * Have we found the PCI root bridge?
 */
static int pciFoundRootBridge;

static void pciScanBus(uint8_t bus, PCIBridge *bridge);

uint32_t pciReadConfigReg(uint32_t addr)
{
	mutexLock(&pciLock);
	outd(PCI_CONFIG_ADDR, addr);
	uint32_t value = ind(PCI_CONFIG_DATA);
	mutexUnlock(&pciLock);

	return value;
};

void pciWriteConfigReg(uint32_t addr, uint32_t value)
{
	mutexLock(&pciLock);
	outd(PCI_CONFIG_ADDR, addr);
	outd(PCI_CONFIG_DATA, value);
	mutexUnlock(&pciLock);
};

static uint8_t pciGetIntVector(uint8_t slot, uint8_t intpin, PCIBridge *bridge)
{
	if (intpin == 0)
	{
		return 0;
	};

	if (bridge == NULL)
	{
		PCIIntRouting *route;
		for (route=pciIntRoutingTable; route!=NULL; route=route->next)
		{
			if (route->slot == slot && route->intpin == intpin)
			{
				return route->vector;
			};
		};

		panic("Failed to find the interrupt mapping!");
	}
	else
	{
		return pciGetIntVector(bridge->masterSlot, ((slot + (intpin-1)) & 3) + 1, bridge->up);
	};
};

static void pciGetDeviceConfigSpace(uint8_t bus, uint8_t slot, uint8_t func, PCIDeviceConfig *config)
{
	uint32_t addr = PCI_REG_ADDR(bus, slot, func, 0);

	uint32_t regno;
	uint32_t *put = config->words;
	for (regno=addr; regno<addr+64; regno+=4)
	{
		*put++ = pciReadConfigReg(regno);
	};
};

static uint32_t pciGetBarSize(uint8_t bus, uint8_t slot, uint8_t func, int i)
{
	uint32_t addr = PCI_REG_ADDR(bus, slot, func, PCI_REG_BAR(i));
	uint32_t oldval = pciReadConfigReg(addr);
	pciWriteConfigReg(addr, 0xFFFFFFFF);
	uint32_t barsz = pciReadConfigReg(addr);
	pciWriteConfigReg(addr, oldval);

	if (barsz & 1)
	{
		barsz &= ~3;
	}
	else
	{
		barsz &= ~0xF;
	};
	
	return (~barsz) + 1;
};

static void pciScanSlot(uint8_t bus, uint8_t slot, PCIBridge *bridge)
{
	PCIDeviceConfig config;
	uint8_t numFuncs = 1;

	pciGetDeviceConfigSpace(bus, slot, 0, &config);
	if (config.std.vendor == PCI_VENDOR_NULL) return;

	if (config.std.headerType & PCI_HEADER_TYPE_MULTIFUNC)
	{
		numFuncs = 8;
	};

	uint8_t type = config.std.headerType & PCI_HEADER_TYPE_MASK;
	if (type == PCI_HEADER_TYPE_PCI_BRIDGE)
	{
		// PCI-to-PCI bridge
		PCIBridge newBridge;
		newBridge.masterSlot = slot;
		newBridge.up = bridge;
		pciScanBus(config.bridge.secondaryBus, &newBridge);
	}
	else if (type == PCI_HEADER_TYPE_NORMAL)
	{
		uint8_t func;
		for (func=0; func<numFuncs; func++)
		{
			pciGetDeviceConfigSpace(bus, slot, func, &config);
			if (config.std.vendor == PCI_VENDOR_NULL) continue;

			uint8_t intVector = pciGetIntVector(slot, config.std.intpin, bridge);

			kprintf("PCI: Found device %04hX:%04hX at PCI[%hhu:%hhu:%hhu] mapped to interrupt %hhu\n",
				config.std.vendor, config.std.device, bus, slot, func, intVector);
			
			PCIDevice *pcidev = (PCIDevice*) kmalloc(sizeof(PCIDevice));
			if (pcidev == NULL)
			{
				panic("Failed to allocate memory for the device!");
			};

			memset(pcidev, 0, sizeof(PCIDevice));

			pcidev->next = pciDeviceHead;
			pciDeviceHead = pcidev;

			pcidev->bus = bus;
			pcidev->slot = slot;
			pcidev->func = func;

			int nextBarIndex = 0;
			while (nextBarIndex < 6)
			{
				uint32_t bar = config.std.bar[nextBarIndex];
				uint32_t barsz = pciGetBarSize(bus, slot, func, nextBarIndex);

				PCI_BAR *barspec = &pcidev->bars[nextBarIndex];
				barspec->barsz = barsz;

				if (barsz == 0)
				{
					nextBarIndex++;
					continue;
				};

				if (bar & 1)
				{
					// I/O space BAR
					uint16_t base = bar & ~3;
					kprintf("  BAR%d at I/O address base 0x%04hX (size 0x%X)\n", nextBarIndex, base, barsz);

					barspec->basePort = base;
					
					nextBarIndex++;
				}
				else
				{
					uint32_t type = bar & 7;
					if (type == 0)
					{
						uint32_t base = bar & ~0xF;
						kprintf("  BAR%d at 32-bit physical base 0x%08X (size 0x%X)\n",
							nextBarIndex, base, barsz);

						barspec->memAddr = pagetabMapPhys(base, barsz, PT_NOCACHE | PT_NOEXEC | PT_WRITE);
						if (barspec->memAddr == NULL)
						{
							panic("Failed to map a memory BAR!");
						};

						nextBarIndex++;
					}
					else if (type == 4)
					{
						uint64_t base = (uint64_t) (bar & ~0xF)
							+ ((uint64_t) config.std.bar[nextBarIndex+1] << 32);
						kprintf("  BAR%d at 64-bit physical base 0x%016lX (size 0x%X)\n",
							nextBarIndex, base, barsz);
						
						barspec->memAddr = pagetabMapPhys(base, barsz, PT_NOCACHE | PT_NOEXEC | PT_WRITE);
						if (barspec->memAddr == NULL)
						{
							panic("Failed to map a memory BAR!");
						};

						nextBarIndex += 2;
					};
				};
			};
		};
	};
};

static void pciScanBus(uint8_t bus, PCIBridge *bridge)
{
	uint8_t slot;
	for (slot=0; slot<32; slot++)
	{
		pciScanSlot(bus, slot, bridge);
	};
};

static void pciIntHandler(void *context)
{
	uint8_t vector = (uint8_t) (uint64_t) context;		// context is vector cast to pointer
	kprintf("PCI interrupt %hhu\n", vector);
};

static void pciMapInterrupt(uint8_t slot, uint8_t intpin, uint8_t vector, int gsi)
{
	kprintf("PCI: Slot %hhu INT%c# mapped to interrupt vector %hhu\n", slot, 'A'+intpin-1, vector);

	PCIIntRouting *route = (PCIIntRouting*) kmalloc(sizeof(PCIIntRouting));
	if (route == NULL)
	{
		panic("Ran out of memory while mapping PCI IRQs!");
	};

	route->slot = slot;
	route->intpin = intpin;
	route->gsi = gsi;
	route->vector = vector;
	route->next = pciIntRoutingTable;
	pciIntRoutingTable = route;

	idtRegisterHandler(vector, pciIntHandler, (void*) (uint64_t) vector);	// context is vector converted to pointer
};

static void pciMapInterruptFromGSI(uint8_t slot, uint8_t intpin, int gsi)
{
	PCIIntRouting *route;
	for (route=pciIntRoutingTable; route!=NULL; route=route->next)
	{
		if (route->gsi == gsi)
		{
			break;
		};
	};

	uint8_t vector;
	if (route == NULL)
	{
		vector = pciNextIntVector++;
		if (vector > I_PCI15)
		{
			panic("Ran out of PCI interrupt vectors!");
		};
	}
	else
	{
		vector = route->vector;
	};

	ioapicMap(gsi, vector, IOAPIC_POLARITY_ACTIVE_LOW, IOAPIC_TRIGGER_MODE_LEVEL);
	pciMapInterrupt(slot, intpin, vector, gsi);
};

static void pciMapInterruptFromIRQ(uint8_t slot, uint8_t intpin, int irq)
{
	pciMapInterrupt(slot, intpin, IRQ0 + irq, -1);
};

static ACPI_STATUS pciWalkCallback(ACPI_HANDLE object, UINT32 nestingLevel, void *context, void **returnvalue)
{
	ACPI_DEVICE_INFO *info;
	ACPI_STATUS status = AcpiGetObjectInfo(object, &info);
	
	if (status != AE_OK)
	{
		panic("AcpiGetObjectInfo failed");
	};
	
	if (info->Flags & ACPI_PCI_ROOT_BRIDGE)
	{
		pciFoundRootBridge = 1;
		
		ACPI_BUFFER prtbuf;
		prtbuf.Length = ACPI_ALLOCATE_BUFFER;
		prtbuf.Pointer = NULL;
		
		status = AcpiGetIrqRoutingTable(object, &prtbuf);
		if (status != AE_OK)
		{
			panic("AcpiGetIrqRoutingTable failed for a root bridge!\n");
		};
		
		char *scan = (char*) prtbuf.Pointer;
		while (1)
		{
			ACPI_PCI_ROUTING_TABLE *table = (ACPI_PCI_ROUTING_TABLE*) scan;
			if (table->Length == 0)
			{
				break;
			};
			
			uint8_t slot = (uint8_t) (table->Address >> 16);
			if (table->Source[0] == 0)
			{
				// static assignment
				pciMapInterruptFromGSI(slot, table->Pin+1, table->SourceIndex);
			}
			else
			{
				// get the PCI Link Object
				ACPI_HANDLE linkObject;
				status = AcpiGetHandle(object, table->Source, &linkObject);
				if (status != AE_OK)
				{
					panic("AcpiGetHandle failed for '%s'", table->Source);
				};
				
				// get the IRQ it is using
				ACPI_BUFFER resbuf;
				resbuf.Length = ACPI_ALLOCATE_BUFFER;
				resbuf.Pointer = NULL;
				
				status = AcpiGetCurrentResources(linkObject, &resbuf);
				if (status != AE_OK)
				{
					panic("AcpiGetCurrentResources failed for '%s'", table->Source);
				};
				
				char *rscan = (char*) resbuf.Pointer;
				int devIRQ = -1;
				while (1)
				{
					ACPI_RESOURCE *res = (ACPI_RESOURCE*) rscan;
					if (res->Type == ACPI_RESOURCE_TYPE_END_TAG)
					{
						break;
					};
					
					if (res->Type == ACPI_RESOURCE_TYPE_IRQ)
					{
						devIRQ = res->Data.Irq.Interrupts[table->SourceIndex];
						break;
					};
					
					if (res->Type == ACPI_RESOURCE_TYPE_EXTENDED_IRQ)
					{
						devIRQ = res->Data.ExtendedIrq.Interrupts[table->SourceIndex];
						break;
					};
					
					rscan += res->Length;
				};
				
				kfree(resbuf.Pointer);
				
				if (devIRQ == -1)
				{
					panic("failed to derive IRQ for device %d from '%s'", (int)slot, table->Source);
				};
				
				pciMapInterruptFromIRQ(slot, table->Pin+1, devIRQ);
			};
			
			scan += table->Length;
		};
		
		kfree(prtbuf.Pointer);
	};
	
	ACPI_FREE(info);
	return AE_OK;
};

static void _pciEnumDevices()
{
	void *retval;
	ACPI_STATUS status = AcpiGetDevices(NULL, pciWalkCallback, NULL, &retval);
	if (status != AE_OK)
	{
		panic("AcpiGetDevices failed");
	};

	if (!pciFoundRootBridge)
	{
		panic("Failed to find the PCI root bridge in ACPI\n");
	};

	pciScanBus(0, NULL);

	// we can now free the list of interrupt mappings as they were assigned to devices
	while (pciIntRoutingTable != NULL)
	{
		PCIIntRouting *route = pciIntRoutingTable;
		pciIntRoutingTable = route->next;

		kfree(route);
	};
};

KERNEL_INIT_ACTION(_pciEnumDevices, KIA_PCI_ENUM_DEVICES, KIA_ACPI_INIT);