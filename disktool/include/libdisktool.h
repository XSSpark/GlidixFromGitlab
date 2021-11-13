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

#ifndef LIBDISKTOOL_
#define	LIBDISKTOOL_

#include <inttypes.h>

/**
 * Minimum buffer size to store a NUL-terminated GUID string.
 */
#define	GUID_STRING_SIZE					37

/**
 * Size of a sector.
 */
#define	DISK_SECTOR_SIZE					512

/**
 * MBR "boot signature".
 */
#define	MBR_SIG					0xAA55

/**
 * MBR partition type for GPT.
 */
#define	MBR_PARTTYPE_GPT			0xEE

/**
 * GPT header signature ("EFI PART").
 */
#define	GPT_SIG					 0x5452415020494645

/**
 * GPT header revision number.
 */
#define	GPT_REVISION				0x10000

/**
 * MBR partition entry.
 */
typedef struct
{
	uint8_t mpAttr;
	uint8_t mpStartCHS[3];
	uint8_t mpType;
	uint8_t mpEndCHS[3];
	uint32_t mpStartLBA;
	uint32_t mpSectorCount;
} __attribute__ ((packed)) MBR_Part;

/**
 * Master Boot Record.
 */
typedef struct
{
	char mbrCode[446];
	MBR_Part mbrParts[4];
	uint16_t mbrSig;
} __attribute__ ((packed)) MBR;

/**
 * GUID/UUID.
 */
typedef union
{
	char bytes[16];
} GUID;

/**
 * GPT header.
 */
typedef struct
{
	uint64_t gptSig;
	uint32_t gptRevision;
	uint32_t gptHeaderSize;
	uint32_t gptHeaderCRC;
	uint32_t gptResv0;
	uint64_t gptThisHeaderLBA;
	uint64_t gptOtherHeaderLBA;
	uint64_t gptFirstDataLBA;
	uint64_t gptLastDataLBA;
	GUID gptDiskGUID;
	uint64_t gptTableStartLBA;
	uint32_t gptNumParts;
	uint32_t gptPartEntrySize;
	uint32_t gptTableCRC;
	uint32_t gptResv1;
} GPT_Header;

/**
 * GPT partition table entry.
 */
typedef struct
{
	GUID gptPartType;
	GUID gptPartGUID;
	uint64_t gptStartLBA;
	uint64_t gptLastLBA;
	uint64_t gptPartFlags;
	uint16_t gptPartName[36];
} GPT_Part;

/**
 * Handle to an open disk image.
 */
typedef struct
{
	/**
	 * The file descriptor for I/O.
	 */
	int fd;
	
	/**
	 * Total number of sectors.
	 */
	uint64_t numSectors;
	
	/**
	 * Primary copy of the header.
	 */
	GPT_Header primaryHeader;
	
	/**
	 * Secondary copy of the header.
	 */
	GPT_Header secondaryHeader;
	
	/**
	 * The partition table itself.
	 */
	GPT_Part *parts;
} Disk;

/**
 * Information about a partition on the disk.
 */
typedef struct
{
	/**
	 * The file descriptor for the disk.
	 */
	int fd;
	
	/**
	 * The byte offset into the file where this partition begins.
	 */
	uint64_t offset;
	
	/**
	 * Number of sectors on the partition.
	 */
	uint64_t numSectors;
	
	/**
	 * Partition type GUID.
	 */
	GUID partType;
	
	/**
	 * Unique partition GUID.
	 */
	GUID partGUID;
} PartInfo;

/**
 * Create a new disk image. 'filename' must be a non-existent file, where the image will be created.
 * 'megabytes' is the size if the file in megabytes, and must be at least 10. Returns the Disk handle
 * on success, or NULL on error, and sets errno. Remember to close the disk handle so the data is
 * actually flushed.
 */
Disk* diskCreate(const char *filename, uint64_t megabytes);

/**
 * Open an existing disk image. 'filename' is the file containing the disk image. Returns the Disk handle
 * on success, or NULL on error, and sets errno.
 */
Disk* diskOpen(const char *filename);

/**
 * Generate a random GUID.
 */
GUID diskGUIDGenerate();

/**
 * Convert a GUID into a string. The 'buffer' will contain the string. The buffer size must be at least
 * GUID_STRING_SIZE bytes.
 */
void diskGUIDToString(GUID guid, char *buffer);

/**
 * Flush the partition table. Returns 0 on success, or -1 on error and sets errno.
 */
int diskFlush(Disk *disk);

/**
 * Flush the partition table and close the disk handle.
 */
void diskClose(Disk *disk);

/**
 * Returns nonzero if the GUID is null (all zeroes).
 */
int diskGUIDIsNull(GUID guid);

/**
 * Parse a GUID string, and store it in the specified pointer. Returns 0 on success, -1 if the string
 * is invalid.
 */
int diskGUIDFromString(GUID *guid, const char *str);

/**
 * Create a new partition on the disk, with the specified type GUID and the specified size in megabytes.
 * Returns 0 on success, -1 if there was no space. Information about the partition is stored in 'partInfo'.
 */
int diskCreatePart(Disk *disk, GUID type, uint64_t megabytes, PartInfo *partInfo);

/**
 * Returns nonzero if the GUIDs match.
 */
int diskGUIDIsEqual(GUID a, GUID b);

/**
 * Delete the partition with the specified GUID. Returns 0 on success, -1 if the partition does not exist.
 */
int diskDeletePart(Disk *disk, GUID partGUID);

/**
 * Get information about the partition with the specified GUID. Returns 0 on success, -1 if the partition does
 * not exist.
 */
int diskGetPartInfo(Disk *disk, GUID partGUID, PartInfo *partInfo);

/**
 * Get information about the partition with the specified index (starting at 0). Returns 0 on success, -1 if the
 * partition does not exist.
 */
int diskGetPartInfoByIndex(Disk *disk, int index, PartInfo *partInfo);

#endif