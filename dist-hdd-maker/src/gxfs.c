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

#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <dirent.h>

#include "gxfs.h"
#include "maker.h"

static GXFS_Superblock superblock;
static off_t gxfsStartPos;
static size_t gxfsPartSize;

static void generateMGSID(uint8_t *buffer)
{
	int fd = open("/dev/urandom", O_RDONLY);
	assert(fd != -1);

	ssize_t sz = read(fd, buffer, 16);
	assert(sz == 16);
	close(fd);
};

static void doChecksum(uint64_t *ptr)
{
	size_t count = 9;		/* 9 quadwords before the sbhChecksum field */
	uint64_t state = 0xF00D1234BEEFCAFEUL;
	
	while (count--)
	{
		state = (state << 1) ^ (*ptr++);
	};
	
	*ptr = state;
};

/**
 * Allocate a new block and return its number.
 */
static uint64_t allocBlock()
{
	if (superblock.body.sbbUsedBlocks == superblock.body.sbbTotalBlocks)
	{
		fprintf(stderr, "[dist-hdd-maker] Ran out of space on the partition!\n");
		exit(1);
	};

	return superblock.body.sbbUsedBlocks++;
};

/**
 * Write data to the block number `blocknum`. `size` must be at most `GXFS_BLOCK_SIZE`.
 */
static void writeBlock(uint64_t blocknum, const void *data, size_t size)
{
	if (pwrite(hdd, data, size, GXFS_BLOCKS_OFFSET + GXFS_BLOCK_SIZE * blocknum) != size)
	{
		fprintf(stderr, "[dist-hdd-maker] Failed to write a block to the disk\n");
		exit(1);
	};
};

/**
 * Flush the inode data to disk.
 */
static void flushInodeWriter(GXFS_InodeWriter *iw)
{
	writeBlock(iw->currentBlockNum, &iw->iBlock, sizeof(iw->iBlock));
};

/**
 * Append a new record to the inode writer. The `size` must be divisible by 8.
 */
static void appendInodeRecord(GXFS_InodeWriter *iw, const void *record, size_t size)
{
	if (size % 8)
	{
		fprintf(stderr, "[dist-hdd-maker] appendInodeRecord() called with a size which is not a multiple of 8!\n");
		exit(1);
	};

	const uint64_t *scan = (const uint64_t*) record;
	size_t wordsLeft = size/8;

	while (wordsLeft--)
	{
		if (iw->nextRecordWord == GXFS_IDATA_WORDS)
		{
			uint64_t ihNext = allocBlock();

			iw->iBlock.ihNext = ihNext;
			flushInodeWriter(iw);

			iw->currentBlockNum = ihNext;
			iw->nextRecordWord = 0;
			memset(&iw->iBlock, 0, sizeof(iw->iBlock));
		};

		iw->iBlock.iDataWords[iw->nextRecordWord++] = *scan++;
	};
};

/**
 * Append a directory entry (`DENT`) record to the inode.
 */
static void appendDent(GXFS_InodeWriter *iw, const char *name, uint64_t ino, uint8_t type)
{
	size_t recSize = sizeof(GXFS_DentRecord) + strlen(name) + 1;
	recSize = (recSize + 7) & ~7;

	GXFS_DentRecord *dent = (GXFS_DentRecord*) malloc(recSize);
	memset(dent, 0, recSize);

	dent->drType = *((const uint32_t*)"DENT");
	dent->drRecordSize = recSize;
	dent->drInode = ino;
	dent->drInoType = type;
	strcpy(dent->drName, name);

	appendInodeRecord(iw, dent, recSize);
	free(dent);
};

/**
 * Get the dentry type to use for the specified path.
 */
static uint8_t getDentType(const char *path)
{
	struct stat st;
	if (lstat(path, &st) != 0)
	{
		perror(path);
		exit(1);
	};

	if (S_ISDIR(st.st_mode))
	{
		return 1;
	}
	else if (S_ISLNK(st.st_mode))
	{
		return 5;
	}
	else
	{
		return 0;
	};
}

/**
 * Get the value of the `arFlags` field in an `ATTR` record for the given `mode`.
 */
static uint32_t getAttrFlags(mode_t mode)
{
	uint32_t perms = mode & 0777;

	if (S_ISDIR(mode))
	{
		return perms | GXFS_TYPE_DIR;
	}
	else if (S_ISLNK(mode))
	{
		return perms | GXFS_TYPE_SYMLINK;
	}
	else
	{
		return perms;
	};
};

/**
 * Initialize the inode writer for the specified inode block number `iBlockNum`. The `st`
 * struct is used to build the `ATTR` record for this inode.
 */
static void initInodeWriter(GXFS_InodeWriter *iw, uint64_t iBlockNum, struct stat *st)
{
	memset(iw, 0, sizeof(GXFS_InodeWriter));
	iw->currentBlockNum = iBlockNum;

	// create the ATTR record
	GXFS_AttrRecord ar;
	ar.arType = (*((const uint32_t*)"ATTR"));
	ar.arRecordSize = sizeof(GXFS_AttrRecord);
	ar.arLinks = 1;
	ar.arFlags = getAttrFlags(st->st_mode);
	ar.arOwner = 0;
	ar.arGroup = 0;
	ar.arSize = st->st_size;
	ar.arATime = time(NULL);
	ar.arMTime = time(NULL);
	ar.arCTime = time(NULL);
	ar.arBTime = time(NULL);
	ar.arANano = 0;
	ar.arMNano = 0;
	ar.arCNano = 0;
	ar.arBNano = 0;
	ar.arIXPerm = 0;
	ar.arOXPerm = 0;
	ar.arDXPerm = 0;
	appendInodeRecord(iw, &ar, sizeof(ar));
};

static uint64_t writeTree(int infd, uint64_t depth)
{
	if (depth != 0)
	{
		uint64_t ents[512];

		int i;
		int anyNonzero = 0;
		for (i=0; i<512; i++)
		{
			ents[i] = writeTree(infd, depth-1);
			if (ents[i] != 0) anyNonzero = 1;
		};

		if (!anyNonzero)
		{
			return 0;
		};

		uint64_t tableBlock = allocBlock();
		writeBlock(tableBlock, ents, GXFS_BLOCK_SIZE);
		return tableBlock;
	}
	else
	{
		char buffer[GXFS_BLOCK_SIZE];
		ssize_t size = read(infd, buffer, GXFS_BLOCK_SIZE);

		if (size == -1)
		{
			perror("read");
			exit(1);
		};

		if (size == 0)
		{
			return 0;
		};

		uint64_t blockNum = allocBlock();
		writeBlock(blockNum, buffer, size);

		return blockNum;
	};
};

/**
 * Make the specified inode from the specified path.
 */
static void makeInode(uint64_t iBlockNum, const char *path)
{
	printf("[dist-hdd-maker] Making inode %lu from `%s'...\n", iBlockNum, path);

	struct stat st;
	if (lstat(path, &st) != 0)
	{
		perror("[dist-hdd-maker] lstat");
		exit(1);
	};

	GXFS_InodeWriter iw;
	initInodeWriter(&iw, iBlockNum, &st);

	if (S_ISDIR(st.st_mode))
	{
		DIR *dirp = opendir(path);
		if (dirp == NULL)
		{
			perror("[dist-hdd-maker] opendir");
			exit(1);
		};

		struct dirent *ent;
		while ((ent = readdir(dirp)) != NULL)
		{
			if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
			{
				// skip '.' and '..'
				continue;
			};

			char fullpath[512];
			snprintf(fullpath, 512, "%s/%s", path, ent->d_name);

			uint64_t newIno = allocBlock();
			appendDent(&iw, ent->d_name, newIno, getDentType(fullpath));
			makeInode(newIno, fullpath);
		};

		closedir(dirp);
	}
	else if (S_ISREG(st.st_mode))
	{
		GXFS_TreeRecord tree;
		tree.trType = *((const uint32_t*)"TREE");
		tree.trSize = sizeof(GXFS_TreeRecord);
		tree.trDepth = 1;
		tree.trHead = 0;

		size_t currentMaxSize = 0x1000;
		while (currentMaxSize < st.st_size)
		{
			tree.trDepth++;
			currentMaxSize <<= 9;
		};

		int fd = open(path, O_RDONLY);
		if (fd == -1)
		{
			perror(path);
			exit(1);
		};

		tree.trHead = writeTree(fd, tree.trDepth);
		appendInodeRecord(&iw, &tree, sizeof(tree));
	}
	else if (S_ISLNK(st.st_mode))
	{
		// TODO
	};

	flushInodeWriter(&iw);
};

void gxfsMake(off_t startPos, size_t size)
{
	gxfsStartPos = startPos;
	gxfsPartSize = size;
	
	printf("[dist-hdd-maker] Initializing the superblock...\n");
	uint64_t formatTime = (uint64_t) time(NULL);

	superblock.header.sbhMagic = GXFS_MAGIC;
	generateMGSID(superblock.header.sbhBootID);
	superblock.header.sbhFormatTime = formatTime;
	superblock.header.sbhWriteFeatures = GXFS_FEATURE_BASE;
	superblock.header.sbhReadFeatures = GXFS_FEATURE_BASE;
	superblock.header.sbhOptionalFeatures = 0;
	doChecksum((uint64_t*) &superblock.header);

	superblock.body.sbbResvBlocks = 8;
	superblock.body.sbbUsedBlocks = 8;
	superblock.body.sbbTotalBlocks = GXFS_NUM_BLOCKS;
	superblock.body.sbbFreeHead = 0;
	superblock.body.sbbLastMountTime = formatTime;
	superblock.body.sbbLastCheckTime = formatTime;
	superblock.body.sbbRuntimeFlags = 0;

	printf("[dist-hdd-maker] Writing the filesystem...\n");
	makeInode(2, "build-sysroot");

	printf("[dist-hdd-maker] Flushing the superblock...\n");
	writeBlock(0, &superblock, sizeof(superblock));

	printf("[dist-hdd-maker] Used %lu/%lu blocks (%luM) (%lu%%)\n",
		superblock.body.sbbUsedBlocks,
		superblock.body.sbbTotalBlocks,
		superblock.body.sbbUsedBlocks/256,
		superblock.body.sbbUsedBlocks * 100 / superblock.body.sbbTotalBlocks
	);
};