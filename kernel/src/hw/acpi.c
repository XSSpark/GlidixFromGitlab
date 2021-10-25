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

#include <glidix/hw/acpi.h>
#include <glidix/util/init.h>
#include <glidix/util/panic.h>
#include <glidix/util/log.h>

#define ACPI_OSC_QUERY_INDEX				0
#define ACPI_OSC_SUPPORT_INDEX				1
#define ACPI_OSC_CONTROL_INDEX				2

#define ACPI_OSC_QUERY_ENABLE				0x1

#define ACPI_OSC_SUPPORT_SB_PR3_SUPPORT			0x4
#define ACPI_OSC_SUPPORT_SB_APEI_SUPPORT		0x10

static const UINT8 uuidOffset[16] =
{
    6,4,2,0,11,9,16,14,19,21,24,26,28,30,32,34
};

static UINT8 hex2num(char hex)
{
	if ((hex >= 'A') && (hex <= 'F'))
	{
		return (UINT8) (hex-'A');
	}
	else
	{
		return (UINT8) (hex-'0');
	};
};

static void str2uuid(char *InString, UINT8 *UuidBuffer)
{
	UINT32                  i;

	for (i = 0; i < UUID_BUFFER_LENGTH; i++)
	{
		UuidBuffer[i] = (hex2num (
			InString[uuidOffset[i]]) << 4);

		UuidBuffer[i] |= hex2num (
			InString[uuidOffset[i] + 1]);
	};
};

static void _acpiInit()
{
	ACPI_STATUS status = AcpiInitializeSubsystem();
	if (ACPI_FAILURE(status))
	{
		panic("AcpiInitializeSubsystem failed");
	};

	AcpiInstallInterface("Windows 2009");
	
	status = AcpiReallocateRootTable();
	if (ACPI_FAILURE(status))
	{
		panic("AcpiReallocateRootTable failed\n");
	};
	
	status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT, ACPI_ADR_SPACE_SYSTEM_MEMORY, ACPI_DEFAULT_HANDLER, NULL, NULL);
	if (ACPI_FAILURE(status))
	{
		panic("AcpiInstallAddressSpaceHandler failed");
	};

	status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT, ACPI_ADR_SPACE_SYSTEM_IO, ACPI_DEFAULT_HANDLER, NULL, NULL);
	if (ACPI_FAILURE(status))
	{
		panic("AcpiInstallAddressSpaceHandler failed");
	};

	status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT, ACPI_ADR_SPACE_PCI_CONFIG, ACPI_DEFAULT_HANDLER, NULL, NULL);
	if (ACPI_FAILURE(status))
	{
		panic("AcpiInstallAddressSpaceHandler failed");
	};

	status = AcpiInitializeTables(NULL, 16, FALSE);
	if (ACPI_FAILURE(status))
	{
		panic("AcpiInitializeTables failed");
	};
	
	kprintf("Loading ACPI tables...\n");
	status = AcpiLoadTables();
	if (ACPI_FAILURE(status))
	{
		panic("AcpiLoadTables failed");
	};
	
	kprintf("Initializing all ACPI subsystems...\n");
	status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(status))
	{
		panic("AcpiEnableSubsystem failed");
	};
	
	kprintf("Initializing ACPI objects...\n");
	status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(status))
	{
		panic("AcpiInitializeObjects failed");
	};

	uint32_t capabilities[2];
	capabilities[ACPI_OSC_QUERY_INDEX] = ACPI_OSC_QUERY_ENABLE;
	capabilities[ACPI_OSC_SUPPORT_INDEX] = ACPI_OSC_SUPPORT_SB_PR3_SUPPORT;
	
	ACPI_OBJECT_LIST input;
	ACPI_OBJECT inParams[4];
	uint8_t uuid[16];
	ACPI_BUFFER output;
	
	str2uuid("0811B06E-4A27-44F9-8D60-3CBBC22E7B48", uuid);
	output.Length = ACPI_ALLOCATE_BUFFER;
	output.Pointer = NULL;
	
	input.Count = 4;
	input.Pointer = inParams;

	inParams[0].Type = ACPI_TYPE_BUFFER;
	inParams[0].Buffer.Length = 16;
	inParams[0].Buffer.Pointer = uuid;

	inParams[1].Type = ACPI_TYPE_INTEGER;
	inParams[1].Integer.Value = 1;

	inParams[2].Type = ACPI_TYPE_INTEGER;
	inParams[2].Integer.Value = 2;

	inParams[3].Type = ACPI_TYPE_BUFFER;
	inParams[3].Buffer.Length = 8;
	inParams[3].Buffer.Pointer = (UINT8*) capabilities;

	ACPI_HANDLE rootHandle;
	if (ACPI_FAILURE(AcpiGetHandle(NULL, "\\_SB", &rootHandle)))
	{
		panic("Failed to get \\_SB object!");
	};
	
	status = AcpiEvaluateObject(rootHandle, "_OSC", &input, &output);

	ACPI_OBJECT arg1;
	arg1.Type = ACPI_TYPE_INTEGER;
	arg1.Integer.Value = 1;		/* IOAPIC */
	
	ACPI_OBJECT_LIST args;
	args.Count = 1;
	args.Pointer = &arg1;
	
	AcpiEvaluateObject(ACPI_ROOT_OBJECT, "_PIC", &args, NULL);
	kprintf("ACPICA init done!\n");
};

KERNEL_INIT_ACTION(_acpiInit, KIA_ACPI_INIT);