/*
	Glidix disk tool

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

#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	SECTORS_PER_MEGABYTE					2048
#define	MEGABYTE						0x100000
#define	SECTOR_SIZE						512

/**
 * The default number of partitions. Selected such that entire GPT takes up exactly
 * 1MB.
 */
#define	DEFAULT_NUM_PARTS					8184

#define CRCPOLY2 0xEDB88320UL  /* left-right reversal */

static uint32_t crc32(const void *data, size_t n)
{
	const uint8_t *c = (const uint8_t*) data;
	int i, j;
	uint32_t r;

	r = 0xFFFFFFFFUL;
	for (i = 0; i < n; i++)
	{
		r ^= c[i];
		for (j = 0; j < 8; j++)
			if (r & 1) r = (r >> 1) ^ CRCPOLY2;
			else       r >>= 1;
	}
	return r ^ 0xFFFFFFFFUL;
}

GUID diskGUIDGenerate()
{
	GUID guid;
	
	int fd = open("/dev/urandom", O_RDONLY);
	assert(fd != -1);
	int bytes = read(fd, &guid, 16);
	assert(bytes == 16);
	close(fd);
	
	return guid;
};

Disk* diskCreate(const char *filename, uint64_t megabytes)
{
	if (megabytes < 10)
	{
		errno = EINVAL;
		return NULL;
	};
	
	int fd = open(filename, O_RDWR | O_CREAT | O_EXCL, 0644);
	if (fd == -1) return NULL;
	
	if (ftruncate(fd, megabytes * MEGABYTE) != 0)
	{
		int errnum = errno;
		close(fd);
		errno = errnum;
		return NULL;
	};
	
	uint64_t totalSectors = SECTORS_PER_MEGABYTE * megabytes;
	
	// create the protective MBR
	MBR mbr;
	memset(&mbr, 0, sizeof(MBR));
	mbr.mbrCode[0] = 0xCD;
	mbr.mbrCode[1] = 0x18;
	
	mbr.mbrParts[0].mpType = MBR_PARTTYPE_GPT;
	mbr.mbrParts[0].mpStartLBA = 1;
	mbr.mbrParts[0].mpSectorCount = totalSectors-1;
	memset(mbr.mbrParts[0].mpEndCHS, 0xFF, 3);
	if (totalSectors > (1UL << 32)) mbr.mbrParts[0].mpSectorCount = 0xFFFFFFFF;
	
	mbr.mbrSig = MBR_SIG;
	if (pwrite(fd, &mbr, sizeof(MBR), 0) != sizeof(MBR))
	{
		int errnum = errno;
		close(fd);
		errno = errnum;
		return NULL;
	};
	
	// set up the Disk structure
	Disk *disk = (Disk*) calloc(1, sizeof(Disk));
	disk->fd = fd;
	disk->numSectors = totalSectors;
	
	GUID guid = diskGUIDGenerate();
	
	// create the partition table in memory
	disk->parts = (GPT_Part*) calloc(DEFAULT_NUM_PARTS, sizeof(GPT_Part));
	
	// set up the primary header
	disk->primaryHeader.gptSig = GPT_SIG;
	disk->primaryHeader.gptRevision = GPT_REVISION;
	disk->primaryHeader.gptHeaderSize = sizeof(GPT_Header);
	disk->primaryHeader.gptThisHeaderLBA = 1;
	disk->primaryHeader.gptOtherHeaderLBA = totalSectors-1;
	disk->primaryHeader.gptFirstDataLBA = MEGABYTE/SECTOR_SIZE;
	disk->primaryHeader.gptLastDataLBA = totalSectors - disk->primaryHeader.gptFirstDataLBA;
	disk->primaryHeader.gptDiskGUID = guid;
	disk->primaryHeader.gptTableStartLBA = 2;
	disk->primaryHeader.gptNumParts = DEFAULT_NUM_PARTS;
	disk->primaryHeader.gptPartEntrySize = sizeof(GPT_Part);
	disk->primaryHeader.gptTableCRC = crc32(disk->parts, DEFAULT_NUM_PARTS * sizeof(GPT_Part));
	
	// set up the secondary header
	memcpy(&disk->secondaryHeader, &disk->primaryHeader, sizeof(GPT_Header));
	disk->secondaryHeader.gptThisHeaderLBA = totalSectors-1;
	disk->secondaryHeader.gptOtherHeaderLBA = 1;
	disk->secondaryHeader.gptTableStartLBA = totalSectors - MEGABYTE/SECTOR_SIZE;
	
	// calculate header checksums
	disk->primaryHeader.gptHeaderCRC = crc32(&disk->primaryHeader, sizeof(GPT_Header));
	disk->secondaryHeader.gptHeaderCRC = crc32(&disk->secondaryHeader, sizeof(GPT_Header));
	
	// done!
	return disk;
};

static int isHeaderOK(GPT_Header *head, uint64_t totalSectors)
{
	if (head->gptSig != GPT_SIG) return 0;
	if (head->gptRevision != GPT_REVISION) return 0;
	if (head->gptHeaderSize != sizeof(GPT_Header)) return 0;
	if (head->gptThisHeaderLBA != 1) return 0;
	if (head->gptOtherHeaderLBA != totalSectors-1) return 0;
	if (head->gptFirstDataLBA != MEGABYTE/SECTOR_SIZE) return 0;
	if (head->gptLastDataLBA != totalSectors - head->gptFirstDataLBA) return 0;
	if (head->gptTableStartLBA != 2) return 0;
	if (head->gptPartEntrySize != sizeof(GPT_Part)) return 0;
	return 1;
};

static int Disk_PartCompare(const void *a, const void *b)
{
	const GPT_Part *partA = (const GPT_Part*) a;
	const GPT_Part *partB = (const GPT_Part*) b;
	
	if (diskGUIDIsNull(partA->gptPartType))
	{
		return 1;
	}
	else if (diskGUIDIsNull(partB->gptPartType))
	{
		return -1;
	}
	else
	{
		return partA->gptStartLBA - partB->gptStartLBA;
	};
};

static void Disk_Sort(Disk *disk)
{
	qsort(disk->parts, disk->primaryHeader.gptNumParts, sizeof(GPT_Part), Disk_PartCompare);
};

Disk* diskOpen(const char *filename)
{
	int fd = open(filename, O_RDWR);
	if (fd == -1) return NULL;
	
	struct stat st;
	if (fstat(fd, &st) != 0)
	{
		int errnum = errno;
		close(fd);
		errno = errnum;
		return NULL;
	};
	
	Disk *disk = (Disk*) calloc(1, sizeof(Disk));
	disk->fd = fd;
	disk->numSectors = st.st_size / SECTOR_SIZE;
	
	// try reading the primary header
	if (pread(fd, &disk->primaryHeader, sizeof(GPT_Header), 512) != sizeof(GPT_Header))
	{
		int errnum = errno;
		close(fd);
		free(disk);
		errno = errnum;
		return NULL;
	};
	
	// check the header
	if (!isHeaderOK(&disk->primaryHeader, disk->numSectors))
	{
		close(fd);
		free(disk);
		errno = EINVAL;
		return NULL;
	};
	
	// generate the secondary header from the primary
	memcpy(&disk->secondaryHeader, &disk->primaryHeader, sizeof(GPT_Header));
	disk->secondaryHeader.gptThisHeaderLBA = disk->numSectors-1;
	disk->secondaryHeader.gptOtherHeaderLBA = 1;
	disk->secondaryHeader.gptTableStartLBA = disk->numSectors - 1 - MEGABYTE/SECTOR_SIZE;
	
	// allocate the partition array in memory
	disk->parts = (GPT_Part*) malloc(sizeof(GPT_Part) * disk->primaryHeader.gptNumParts);
	if (pread(fd, disk->parts, sizeof(GPT_Part) * disk->primaryHeader.gptNumParts, 1024) != sizeof(GPT_Part) * disk->primaryHeader.gptNumParts)
	{
		int errnum = errno;
		free(disk);
		close(fd);
		errno = errnum;
		return NULL;
	};
	
	// sort the partitions
	Disk_Sort(disk);
	
	// done!
	return disk;
};

int diskFlush(Disk *disk)
{
	// recompute the partition array checksum
	disk->primaryHeader.gptTableCRC = crc32(disk->parts, disk->primaryHeader.gptNumParts * sizeof(GPT_Part));
	disk->secondaryHeader.gptTableCRC = disk->primaryHeader.gptTableCRC;
	
	// recompute the primary header checksum
	disk->primaryHeader.gptHeaderCRC = 0;
	disk->primaryHeader.gptHeaderCRC = crc32(&disk->primaryHeader, sizeof(GPT_Header));
	
	// recompute the secondary header checksum
	disk->secondaryHeader.gptHeaderCRC = 0;
	disk->secondaryHeader.gptHeaderCRC = crc32(&disk->secondaryHeader, sizeof(GPT_Header));
	
	// get the size of the partition table
	uint64_t tableSize = disk->primaryHeader.gptNumParts * sizeof(GPT_Part);
	
	// flush the primary header
	if (pwrite(disk->fd, &disk->primaryHeader, sizeof(GPT_Header), 512) != sizeof(GPT_Header))
	{
		return -1;
	};
	
	// flush the secondary header
	if (pwrite(disk->fd, &disk->secondaryHeader, sizeof(GPT_Header), SECTOR_SIZE * disk->secondaryHeader.gptThisHeaderLBA) != sizeof(GPT_Header))
	{
		return -1;
	};
	
	// flush the primary partition table
	if (pwrite(disk->fd, disk->parts, tableSize, 1024) != tableSize)
	{
		return -1;
	};
	
	// flush the secondary partition table
	if (pwrite(disk->fd, disk->parts, tableSize, SECTOR_SIZE * disk->secondaryHeader.gptTableStartLBA) != tableSize)
	{
		return -1;
	};
	
	// done!
	return 0;
};

void diskClose(Disk *disk)
{
	diskFlush(disk);
	close(disk->fd);
	free(disk->parts);
	free(disk);
};

void diskGUIDToString(GUID guid, char *buffer)
{
	sprintf(buffer, "%02hhX%02hhX%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		// first group in little-endian
		guid.bytes[3], guid.bytes[2], guid.bytes[1], guid.bytes[0],
		// second group in little-endian
		guid.bytes[5], guid.bytes[4],
		// third group in little-endian
		guid.bytes[7], guid.bytes[6],
		// fourth group in big-endian
		guid.bytes[8], guid.bytes[9],
		// final group in big-endian
		guid.bytes[10], guid.bytes[11], guid.bytes[12], guid.bytes[13], guid.bytes[14], guid.bytes[15]
	);
};

int diskGUIDFromString(GUID *guid, const char *str)
{
	int count = sscanf(str, "%02hhX%02hhX%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		// first group in little-endian
		&guid->bytes[3], &guid->bytes[2], &guid->bytes[1], &guid->bytes[0],
		// second group in little-endian
		&guid->bytes[5], &guid->bytes[4],
		// third group in little-endian
		&guid->bytes[7], &guid->bytes[6],
		// fourth group in big-endian
		&guid->bytes[8], &guid->bytes[9],
		// final group in big-endian
		&guid->bytes[10], &guid->bytes[11], &guid->bytes[12], &guid->bytes[13], &guid->bytes[14], &guid->bytes[15]
	);
	
	if (count == 16) return 0;
	else return -1;
};

int diskGUIDIsNull(GUID guid)
{
	int i;
	for (i=0; i<16; i++)
	{
		if (guid.bytes[i] != 0) return 0;
	};
	
	return 1;
};

static int getPartNumberStartingAt(Disk *disk, uint64_t start)
{
	int i;
	for (i=0; i<disk->primaryHeader.gptNumParts; i++)
	{	
		GPT_Part *part = &disk->parts[i];
		if (!diskGUIDIsNull(part->gptPartType) && part->gptStartLBA >= start) break;
	};
	
	return i;
};

static uint64_t getFreeBlocksStartingFrom(Disk *disk, uint64_t start)
{
	int index = getPartNumberStartingAt(disk, start);
	if (index == disk->primaryHeader.gptNumParts)
	{
		// no partitions past this point; the rest of the disk is free
		return disk->primaryHeader.gptLastDataLBA - start + 1;
	}
	else
	{
		GPT_Part *part = &disk->parts[index];
		return part->gptStartLBA - start;
	};
};

int diskCreatePart(Disk *disk, GUID type, uint64_t megabytes, PartInfo *partInfo)
{
	uint64_t numSectors = megabytes * SECTORS_PER_MEGABYTE;
	uint64_t startSector = disk->primaryHeader.gptFirstDataLBA;
	
	while (startSector != disk->primaryHeader.gptLastDataLBA+1)
	{
		uint64_t freeSpace = getFreeBlocksStartingFrom(disk, startSector);
		if (freeSpace >= numSectors) break;
		
		int index = getPartNumberStartingAt(disk, startSector);
		if (index == disk->primaryHeader.gptNumParts) return -1;
		GPT_Part *part = &disk->parts[index];
		startSector = part->gptLastLBA + 1;
	};
	
	if (startSector == disk->primaryHeader.gptLastDataLBA+1)
	{
		// not enough free space!
		return -1;
	};
	
	// find a free entry on the partition table
	int i;
	for (i=0; i<disk->primaryHeader.gptNumParts; i++)
	{
		if (diskGUIDIsNull(disk->parts[i].gptPartType)) break;
	};
	
	if (i == disk->primaryHeader.gptNumParts)
	{
		// no free entries on the partition table
		return -1;
	};
	
	// OK, create the partition
	partInfo->fd = disk->fd;
	partInfo->offset = DISK_SECTOR_SIZE * startSector;
	partInfo->numSectors = numSectors;
	partInfo->partType = type;
	partInfo->partGUID = diskGUIDGenerate();
	
	GPT_Part *part = &disk->parts[i];
	part->gptPartType = type;
	part->gptPartGUID = partInfo->partGUID;
	part->gptStartLBA = startSector;
	part->gptLastLBA = startSector + numSectors - 1;
	part->gptPartFlags = 0;
	memset(part->gptPartName, 0, sizeof(part->gptPartName));
	
	Disk_Sort(disk);
	return 0;
};

int diskGUIDIsEqual(GUID a, GUID b)
{
	return memcmp(&a, &b, 16) == 0;
};

int diskDeletePart(Disk *disk, GUID partGUID)
{
	if (diskGUIDIsNull(partGUID)) return -1;
	
	int i;
	for (i=0; i<disk->primaryHeader.gptNumParts; i++)
	{
		GPT_Part *part = &disk->parts[i];
		if (diskGUIDIsEqual(partGUID, part->gptPartGUID))
		{
			memset(part, 0, sizeof(GPT_Part));
			Disk_Sort(disk);
			return 0;
		};
	};
	
	return -1;
};

int diskGetPartInfo(Disk *disk, GUID partGUID, PartInfo *partInfo)
{
	if (diskGUIDIsNull(partGUID)) return -1;
	
	int i;
	for (i=0; i<disk->primaryHeader.gptNumParts; i++)
	{
		GPT_Part *part = &disk->parts[i];
		if (diskGUIDIsEqual(partGUID, part->gptPartGUID))
		{
			partInfo->fd = disk->fd;
			partInfo->offset = DISK_SECTOR_SIZE * part->gptStartLBA;
			partInfo->numSectors = part->gptLastLBA - part->gptStartLBA + 1;
			partInfo->partType = part->gptPartType;
			partInfo->partGUID = part->gptPartGUID;
			return 0;
		};
	};
	
	return -1;
};

int diskGetPartInfoByIndex(Disk *disk, int index, PartInfo *partInfo)
{
	int i;
	for (i=0; i<disk->primaryHeader.gptNumParts; i++)
	{
		GPT_Part *part = &disk->parts[i];
		if (!diskGUIDIsNull(part->gptPartGUID) && (index--) == 0)
		{
			partInfo->fd = disk->fd;
			partInfo->offset = DISK_SECTOR_SIZE * part->gptStartLBA;
			partInfo->numSectors = part->gptLastLBA - part->gptStartLBA + 1;
			partInfo->partType = part->gptPartType;
			partInfo->partGUID = part->gptPartGUID;
			return 0;
		};
	};
	
	return -1;
};