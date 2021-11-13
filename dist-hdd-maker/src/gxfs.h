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

#ifndef GXFS_H_
#define GXFS_H_

#include <stdint.h>

#include "maker.h"

#define	VBR_SIZE				(2 * 1024 * 1024)
#define	GXFS_MAGIC				(*((const uint64_t*)"__GXFS__"))
#define	GXFS_FEATURE_BASE			(1 << 0)
#define	GXFS_BLOCK_SIZE				0x1000
#define	GXFS_NUM_BLOCKS				((gxfsPartSize-VBR_SIZE)/GXFS_BLOCK_SIZE)
#define	GXFS_BLOCKS_OFFSET			(gxfsStartPos+VBR_SIZE)
#define	GXFS_IDATA_WORDS			((GXFS_BLOCK_SIZE - 8)/8)

#define	GXFS_TYPE_DIR				0x1000
#define	GXFS_TYPE_SYMLINK			0x5000

typedef struct
{
	uint64_t sbhMagic;
	uint8_t  sbhBootID[16];
	uint64_t sbhFormatTime;
	uint64_t sbhWriteFeatures;
	uint64_t sbhReadFeatures;
	uint64_t sbhOptionalFeatures;
	uint64_t sbhResv[2];
	uint64_t sbhChecksum;
} GXFS_SuperblockHeader;

typedef struct
{
	uint64_t sbbResvBlocks;
	uint64_t sbbUsedBlocks;
	uint64_t sbbTotalBlocks;
	uint64_t sbbFreeHead;
	uint64_t sbbLastMountTime;
	uint64_t sbbLastCheckTime;
	uint64_t sbbRuntimeFlags;
} GXFS_SuperblockBody;

typedef struct
{
	uint32_t arType;	/* "ATTR" */
	uint32_t arRecordSize;	/* sizeof(GXFS_AttrRecord) */
	uint64_t arLinks;
	uint32_t arFlags;
	uint16_t arOwner;
	uint16_t arGroup;
	uint64_t arSize;
	uint64_t arATime;
	uint64_t arMTime;
	uint64_t arCTime;
	uint64_t arBTime;
	uint32_t arANano;
	uint32_t arMNano;
	uint32_t arCNano;
	uint32_t arBNano;
	uint64_t arIXPerm;
	uint64_t arOXPerm;
	uint64_t arDXPerm;
} GXFS_AttrRecord;

typedef struct
{
	uint32_t drType;	/* "DENT" */
	uint32_t drRecordSize;	/* sizeof(GXFS_DentRecord) + strlen(filename), 8-byte-aligned */
	uint64_t drInode;
	uint8_t drInoType;
	char drName[];
} GXFS_DentRecord;

typedef struct
{
	uint32_t trType;	/* "TREE" */
	uint32_t trSize;	/* sizeof(GXFS_TreeRecord) */
	uint64_t trDepth;
	uint64_t trHead;
} GXFS_TreeRecord;

typedef struct
{
	GXFS_SuperblockHeader header;
	GXFS_SuperblockBody body;
} GXFS_Superblock;

typedef struct
{
	uint64_t ihNext;
	uint64_t iDataWords[GXFS_IDATA_WORDS];
} GXFS_InodeBlock;

/**
 * Represents the state of an inode writer.
 */
typedef struct
{
	/**
	 * The current block number (where the current inode block is to be written).
	 */
	uint64_t currentBlockNum;

	/**
	 * Content of the current inode block (to be flushed).
	 */
	GXFS_InodeBlock iBlock;

	/**
	 * Index into the `iDataWords` array of the inode block, where the next record
	 * should be written.
	 */
	size_t nextRecordWord;
} GXFS_InodeWriter;

/**
 * Create the GXFS partition.
 */
void gxfsMake(off_t startPos, size_t size);

#endif