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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static const char *progName;

typedef struct
{
	const char *name;
	int (*implement)(int argc, char **argv);
	const char *help;
} Command;

typedef struct
{
	const char *name;
	const char *guidstr;
} PartTypeMapping;

static PartTypeMapping partTypes[] = {
	{"efisys",		"C12A7328-F81F-11D2-BA4B-00A0C93EC93B"},
	{"glidix-root",		"81C1AD9C-BDC4-4809-8D9F-DCB2A9B85D01"},
	{"glidix-data",		"7DAD52A2-C9E2-4B80-85DB-D2BF9A6BE67D"},
	
	// LIST TERMINATOR
	{NULL, NULL}
};

static const char *getTypeName(const char *guidstr)
{
	PartTypeMapping *mapping;
	for (mapping=partTypes; mapping->guidstr!=NULL; mapping++)
	{
		if (strcmp(mapping->guidstr, guidstr) == 0)
		{
			return mapping->name;
		};
	};

	return guidstr;
};

static int getTypeGUID(const char *name, GUID *out)
{
	PartTypeMapping *mapping;
	for (mapping=partTypes; mapping->guidstr!=NULL; mapping++)
	{
		if (strcmp(mapping->name, name) == 0)
		{
			return diskGUIDFromString(out, mapping->guidstr);
		};
	};

	return diskGUIDFromString(out, name);
};

static int cmdCreateDisk(int argc, char **argv)
{
	if (argc != 3)
	{
		fprintf(stderr, "SYNTAX:\n\t%s --create-disk <disk-image-name> <size-in-megs>\n", progName);
		return 1;
	};

	const char *filename = argv[1];
	int megabytes = atoi(argv[2]);
	if (megabytes < 10)
	{
		fprintf(stderr, "%s: the disk size must be at least 10 megabytes\n", progName);
		return 1;
	};

	Disk *disk = diskCreate(filename, megabytes);
	if (disk == NULL)
	{
		fprintf(stderr, "%s: failed to create disk %s: %s\n", progName, filename, strerror(errno));
		return 1;
	};

	diskClose(disk);
	return 0;
};

static int cmdListParts(int argc, char **argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "SYNTAX:\n\t%s --list <disk-image-name>\n", progName);
		return 1;
	};

	Disk *disk = diskOpen(argv[1]);
	if (disk == NULL)
	{
		fprintf(stderr, "%s: failed to open disk %s: %s\n", progName, argv[1], strerror(errno));
		return 1;
	};

	printf("|%-36s|%-36s|%-20s|\n", "GUID", "Type", "Size");

	char bar[36+36+20+4+1];
	memset(bar, '-', sizeof(bar));
	bar[sizeof(bar)-1] = 0;

	printf("%s\n", bar);

	int i;
	PartInfo pinfo;
	for (i=0; diskGetPartInfoByIndex(disk, i, &pinfo)==0; i++)
	{
		char guid[GUID_STRING_SIZE];
		diskGUIDToString(pinfo.partGUID, guid);

		char typeguid[GUID_STRING_SIZE];
		diskGUIDToString(pinfo.partType, typeguid);

		printf("|%36s|%-36s|%19luM|\n", guid, getTypeName(typeguid), pinfo.numSectors / 2048);
	};

	printf("%s\n", bar);
	printf("Total partitions: %d\n", i);
	return 0;
};

static int cmdCreatePart(int argc, char **argv)
{
	if (argc != 4)
	{
		fprintf(stderr, "SYNTAX:\t%s --create-part <disk-image-name> <type> <size-in-megs>\n", progName);
		return 1;
	};

	Disk *disk = diskOpen(argv[1]);
	if (disk == NULL)
	{
		fprintf(stderr, "%s: failed to open %s: %s\n", progName, argv[1], strerror(errno));
		return 1;
	};

	GUID type;
	if (getTypeGUID(argv[2], &type) != 0)
	{
		fprintf(stderr, "%s: invalid partition type: %s\n", progName, argv[2]);
		return 1;
	};

	int megabytes = atoi(argv[3]);
	if (megabytes < 2)
	{
		fprintf(stderr, "%s: partition must be at least 2 megabytes\n", progName);
		return 1;
	};

	PartInfo pinfo;
	if (diskCreatePart(disk, type, megabytes, &pinfo) != 0)
	{
		fprintf(stderr, "%s: ran out of space to create the requested partition\n", progName);
		return 1;
	};

	diskClose(disk);

	char result[GUID_STRING_SIZE];
	diskGUIDToString(pinfo.partGUID, result);
	printf("%s\n", result);
	return 0;
};

static int cmdDeletePart(int argc, char **argv)
{
	if (argc != 3)
	{
		fprintf(stderr, "SYNTAX:\t%s --delete-part <disk-image-name> <part-guid>\n", progName);
		return 1;
	};

	Disk *disk = diskOpen(argv[1]);
	if (disk == NULL)
	{
		fprintf(stderr, "%s: failed to open disk image %s: %s\n", progName, argv[1], strerror(errno));
		return 1;
	};

	GUID guid;
	if (diskGUIDFromString(&guid, argv[2]) != 0)
	{
		fprintf(stderr, "%s: invalid GUID: %s\n", progName, argv[2]);
		return 1;
	};

	if (diskDeletePart(disk, guid) != 0)
	{
		fprintf(stderr, "%s: partition with GUID %s does not exist\n", progName, argv[2]);
		return 1;
	};

	diskClose(disk);
	return 0;
};

static int cmdFirstOfType(int argc, char **argv)
{
	if (argc != 3)
	{
		fprintf(stderr, "SYNTAX:\t%s --first-of-type <disk-image-name> <type>\n", progName);
		return 1;
	};

	GUID result;
	memset(&result, 0, sizeof(GUID));

	Disk *disk = diskOpen(argv[1]);
	if (disk == NULL)
	{
		fprintf(stderr, "%s: failed to open disk %s: %s\n", progName, argv[1], strerror(errno));
		return 1;
	};

	GUID type;
	if (getTypeGUID(argv[2], &type) != 0)
	{
		fprintf(stderr, "%s: invalid partition type: %s\n", progName, argv[2]);
		return 1;
	};

	PartInfo pinfo;
	int i;
	
	for (i=0; diskGetPartInfoByIndex(disk, i, &pinfo)==0; i++)
	{
		if (diskGUIDIsEqual(type, pinfo.partType))
		{
			result = pinfo.partGUID;
			break;
		};
	};

	char guidstr[GUID_STRING_SIZE];
	diskGUIDToString(result, guidstr);

	printf("%s\n", guidstr);
	return 0;
};

static int cmdWrite(int argc, char **argv)
{
	if (argc != 4)
	{
		fprintf(stderr, "SYNTAX:\t%s --write <disk-image-name> <part-guid> <source-file>\n", progName);
		return 1;
	};

	Disk *disk = diskOpen(argv[1]);
	if (disk == NULL)
	{
		fprintf(stderr, "%s: failed to open disk %s: %s\n", progName, argv[1], strerror(errno));
		return 1;
	};

	GUID guid;
	if (diskGUIDFromString(&guid, argv[2]) != 0)
	{
		fprintf(stderr, "%s: invalid GUID: %s\n", progName, argv[2]);
		return 1;
	};

	int fd = open(argv[3], O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "%s: cannot open %s for reading: %s\n", progName, argv[3], strerror(errno));
		return 1;
	};

	PartInfo pinfo;
	if (diskGetPartInfo(disk, guid, &pinfo) != 0)
	{
		fprintf(stderr, "%s: partition %s not found\n", progName, argv[2]);
		return 1;
	};

	off_t pos = pinfo.offset;
	size_t sizeLeft = pinfo.numSectors * 512;

	static char buffer[2 * 1024 * 1024];
	ssize_t chunk;

	while ((chunk = read(fd, buffer, sizeof(buffer))) > 0)
	{
		if (sizeLeft < chunk)
		{
			fprintf(stderr, "%s: source file %s is larger than partition\n", progName, argv[3]);
			return 1;
		};

		if (pwrite(disk->fd, buffer, chunk, pos) != chunk)
		{
			fprintf(stderr, "%s: error while writing to disk image\n", progName);
			return 1;
		};

		sizeLeft -= chunk;
		pos += chunk;
	};

	if (chunk == -1)
	{
		fprintf(stderr, "%s: failed to read: %s\n", progName, strerror(errno));
		return 1;
	};

	close(fd);
	diskClose(disk);
	return 0;
};

static Command cmdList[] = {
	{"--create-disk", cmdCreateDisk, "<disk-image-name> <size-in-megs>\t# Create a disk image with a blank GPT"},
	{"--create-part", cmdCreatePart, "<disk-image-name> <type> <size-in-megs>\t# Create a partition and print its GUID and a newline"},
	{"--delete-part", cmdDeletePart, "<disk-image-name> <part-guid>\t# Deletes the partition with the specified GUID."},
	{"--first-of-type", cmdFirstOfType, "<disk-image-name> <type>\t# Print the GUID of the first partition of the specified type"},
	{"--list", cmdListParts, "<disk-image-name>\t# List partitions on the specified disk image"},
	{"--write", cmdWrite, "<disk-image-name> <part-guid> <source-file>\t# Copy the source file over the contents of the specified partition"},

	// LIST TERMINATOR
	{NULL, NULL, NULL}
};

int main(int argc, char *argv[])
{
	progName = argv[0];

	if (argc < 2)
	{
		fprintf(stderr, "SYNTAX:\n");

		Command *cmd;
		for (cmd=cmdList; cmd->name!=NULL; cmd++)
		{
			fprintf(stderr, "\t%s %s %s\n", argv[0], cmd->name, cmd->help);
		};

		return 1;
	};

	const char *cmdname = argv[1];
	Command *cmd;

	for (cmd=cmdList; cmd->name!=NULL; cmd++)
	{
		if (strcmp(cmd->name, cmdname) == 0)
		{
			break;
		};
	};

	if (cmd->name == NULL)
	{
		fprintf(stderr, "%s: unknown command: `%s'\n", argv[0], cmdname);
		return 1;
	};

	return cmd->implement(argc-1, &argv[1]);
};