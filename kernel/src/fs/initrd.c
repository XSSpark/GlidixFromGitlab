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

#include <glidix/fs/file.h>
#include <glidix/util/init.h>
#include <glidix/fs/initrd.h>
#include <glidix/util/log.h>
#include <glidix/util/panic.h>
#include <glidix/util/string.h>

SECTION(".initrd") uint8_t initrdImage[32*1024*1024];

static uint64_t parseOct(const char *data)
{
	uint64_t out = 0;
	while (*data != 0)
	{
		out = out * 8 + ((*data++)-'0');
	};
	return out;
};

static void initrdInit(KernelBootInfo *info)
{
	kprintf("initrd: Unpacking the initrd...\n");
	if (vfsCreateDirectory(NULL, "/initrd", 0755) != 0)
	{
		panic("Failed to create /initrd!");
	};

	TarHeader *header = (TarHeader*) initrdImage;
	TarHeader *end = (TarHeader*) ((uint64_t)initrdImage + bootInfo->initrdSize);
	
	while (header < end)
	{
		if (header->filename[0] == 0)
		{
			break;
		};
		
		char *data = (char*) &header[1];
		uint64_t size = parseOct(header->size);
		uint64_t asize = (size + 511) & ~511;
		
		char fullpath[256];
		strcpy(fullpath, "/initrd/");
		strcat(fullpath, header->filename);
		
		if (fullpath[strlen(fullpath)-1] == '/')
		{
			fullpath[strlen(fullpath)-1] = 0;

			kprintf("initrd: Creating directory %s...\n", fullpath);
			if (vfsCreateDirectory(NULL, fullpath, 0755) != 0)
			{
				panic("Failed to create directory %s!", fullpath);
			};
		}
		else
		{
			kprintf("initrd: Unpacking file %s...\n", fullpath);

			File *fp = vfsOpen(NULL, fullpath, O_WRONLY | O_CREAT | O_EXCL, 0755, NULL);
			if (fp == NULL)
			{
				panic("Failed to write to file %s!", fullpath);
			};
			
			if (vfsWrite(fp, data, size) != size)
			{
				panic("Failed to write!");
			};

			vfsClose(fp);
		};
		
		header = (TarHeader*) (data + asize);
	};
};

KERNEL_INIT_ACTION(initrdInit, KIA_INITRD, KAI_VFS_KERNEL_ROOT);