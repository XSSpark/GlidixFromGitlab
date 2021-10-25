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

/**
 * The lock which controls access to PCI shared structures and 
 */
static Mutex pciLock;

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

static void _pciEnumDevices()
{

};

KERNEL_INIT_ACTION(_pciEnumDevices, KIA_PCI_ENUM_DEVICES, KIA_ACPI_INIT);