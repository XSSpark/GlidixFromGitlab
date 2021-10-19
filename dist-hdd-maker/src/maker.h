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

#ifndef MAKER_H_
#define MAKER_H_

/**
 * The desired HDD size.
 */
#define	HDD_SIZE			(10ULL * 1024ULL * 1024ULL * 1024ULL)

/**
 * Size of the MBR bootstrap area.
 */
#define	MBR_BOOTSTRAP_SIZE		446

/**
 * Starting LBA of the root partition (1MB into disk).
 */
#define	ROOT_START_LBA			0x800

/**
 * The size of a sector..
 */
#define	SECTOR_SIZE			512

/**
 * Number of sectors for the root partition.
 */
#define	ROOT_NUM_SECTORS		((HDD_SIZE / SECTOR_SIZE) - ROOT_START_LBA)

/**
 * Where to get the MBR bootstrap code.
 */
#define	MBR_PATH			"gxboot-build/gxboot/boot/gxboot/mbr.bin"

/**
 * Path to the VBR bootstrap code.
 */
#define	VBR_PATH			"gxboot-build/gxboot/boot/gxboot/vbr-gxfs.bin"

/**
 * MBR "signature".
 */
#define	MBR_SIG				0xAA55

/**
 * Size of the GXFS VBR (2MB).
 */
#define	VBR_SIZE			(2 * 1024 * 1024)

/**
 * File descriptor for the hard drive image.
 */
extern int hdd;

#endif