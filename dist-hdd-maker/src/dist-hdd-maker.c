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

#include <libdisktool.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "maker.h"
#include "gxfs.h"

int hdd;

int main()
{
	printf("[dist-hdd-maker] Preparing image...\n");
	Disk *disk = diskOpen("distro-out/hdd.bin");
	if (disk == NULL)
	{
		perror("[dist-hdd-maker] diskOpen");
		return 1;
	};

	GUID guidRoot;
	diskGUIDFromString(&guidRoot, "81C1AD9C-BDC4-4809-8D9F-DCB2A9B85D01");

	int i;
	PartInfo pinfo;
	int found = 0;

	for (i=0; diskGetPartInfoByIndex(disk, i, &pinfo)==0; i++)
	{
		if (diskGUIDIsEqual(pinfo.partType, guidRoot))
		{
			found = 1;
			break;
		};
	};

	if (!found)
	{
		fprintf(stderr, "[dist-hdd-maker] Failed to find the root partition!\n");
		return 1;
	};

	hdd = disk->fd;
	gxfsMake(pinfo.offset, pinfo.numSectors * SECTOR_SIZE);

	close(hdd);
	return 0;
};