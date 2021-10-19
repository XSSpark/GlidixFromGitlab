/*
	Glidix Distro HDD Maker

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

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "maker.h"
#include "gxfs.h"

typedef struct
{
	uint8_t flags;
	uint8_t startHead;
	uint16_t startCylSector;
	uint8_t systemID;
	uint8_t endHead;
	uint16_t endCylSector;
	uint32_t startLBA;
	uint32_t partSize;
} __attribute__ ((packed)) MBRPart;

typedef struct
{
	char bootstrap[MBR_BOOTSTRAP_SIZE];
	MBRPart parts[4];
	uint16_t sig;
} __attribute__ ((packed)) MBR;

int hdd;
static uint8_t vbrBuffer[VBR_SIZE];

int main()
{
	printf("[dist-hdd-maker] Creating blank image...\n");

	hdd = open("distro-out/hdd.bin", O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (hdd == -1)
	{
		perror("[dist-hdd-maker] distro-out/hdd.bin");
		return 1;
	};

	if (ftruncate(hdd, HDD_SIZE) != 0)
	{
		perror("[dist-hdd-maker] ftruncate");
		return 1;
	};

	printf("[dist-hdd-maker] Creating MBR...\n");

	MBR mbr;
	memset(&mbr, 0, sizeof(MBR));
	
	int fd = open(MBR_PATH, O_RDONLY);
	if (fd == -1)
	{
		perror("[dist-hdd-maker] " MBR_PATH);
		return 1;
	};

	if (read(fd, mbr.bootstrap, MBR_BOOTSTRAP_SIZE) == -1)
	{
		fprintf(stderr, "[hdd-dist-maker] Failed to read MBR bootstrap code!\n");
		return 1;
	};

	close(fd);

	MBRPart *part = &mbr.parts[0];
	part->flags = 0x80;			// active (bootable) flag set
	part->systemID = 0x7F;			// GXFS partitions to be marked with 'misc' type
	part->startLBA = ROOT_START_LBA;
	part->partSize = ROOT_NUM_SECTORS;

	mbr.sig = MBR_SIG;

	if (pwrite(hdd, &mbr, sizeof(MBR), 0) != sizeof(MBR))
	{
		fprintf(stderr, "[dist-hdd-maker] Failed to write MBR!\n");
		return 1;
	};

	close(fd);

	printf("[dist-hdd-maker] Creating VBR...\n");

	fd = open(VBR_PATH, O_RDONLY);
	if (fd == -1)
	{
		perror("[dist-hdd-maker] " VBR_PATH);
		return 1;
	};

	if (read(fd, vbrBuffer, VBR_SIZE) == -1)
	{
		fprintf(stderr, "[dist-hdd-maker] Failed to read VBR!\n");
		return 1;
	};

	close(fd);

	if (pwrite(hdd, vbrBuffer, VBR_SIZE, ROOT_START_LBA*SECTOR_SIZE) != VBR_SIZE)
	{
		fprintf(stderr, "[dist-hdd-maker] Failed to write VBR!\n");
		return 1;
	};

	gxfsMake();

	close(hdd);
	return 0;
};