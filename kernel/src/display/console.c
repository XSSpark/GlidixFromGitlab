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

#include <glidix/display/console.h>
#include <glidix/thread/spinlock.h>
#include <glidix/util/init.h>
#include <glidix/display/confont.h>
#include <glidix/util/string.h>
#include <glidix/hw/kom.h>
#include <glidix/util/memory.h>
#include <glidix/hw/pagetab.h>
#include <glidix/util/panic.h>

/**
 * The base console color.
 */
#define	CON_BASE_COLOR					0xC0C0C0C0

/**
 * Console margin.
 */
#define	CON_MARGIN					2

/**
 * Width and height of a console character in pixels.
 */
#define	CON_CHAR_WIDTH					9
#define	CON_CHAR_HEIGHT					16

/**
 * The spinlock controlling access to the console.
 */
static Spinlock conLock;

/**
 * Address of the front and back buffers.
 */
static uint8_t *conFrontBuffer;
static uint8_t *conBackBuffer;

/**
 * Size of the console in pixels.
 */
static int conPixelWidth;
static int conPixelHeight;

/**
 * Size of the console in characters.
 */
static int conWidth;
static int conHeight;

/**
 * Number of bytes in a pixel, and in a scanline.
 */
static size_t conPixelSize;
static size_t conScanlineSize;

/**
 * Character position within the console.
 */
static int conPosX;
static int conPosY;

void conInit()
{
	conFrontBuffer = bootInfo->framebuffer;
	conBackBuffer = bootInfo->backbuffer;
	conPixelWidth = bootInfo->fbWidth;
	conPixelHeight = bootInfo->fbHeight;
	conPixelSize = bootInfo->fbFormat.bpp + bootInfo->fbFormat.pixelSpacing;
	conScanlineSize = conPixelSize * bootInfo->fbWidth + bootInfo->fbFormat.scanlineSpacing;

	conWidth = (conPixelWidth - 2 * CON_MARGIN) / CON_CHAR_WIDTH;
	conHeight = (conPixelHeight - 2 * CON_MARGIN) / CON_CHAR_HEIGHT;
};

static void conRenderChar(int posX, int posY, char c)
{
	const uint8_t *fetch = &confont[16*(unsigned int)(unsigned char)c];
	int startX = conPosX * CON_CHAR_WIDTH + CON_MARGIN;
	int startY = conPosY * CON_CHAR_HEIGHT + CON_MARGIN;

	int plotY;
	for (plotY=0; plotY<CON_CHAR_HEIGHT; plotY++)
	{
		uint32_t *put = (uint32_t*) (conFrontBuffer + conScanlineSize * (startY+plotY) + conPixelSize * startX);
		uint32_t *backPut = (uint32_t*) (conBackBuffer + conScanlineSize * (startY+plotY) + conPixelSize * startX);

		int i;
		for (i=0; i<CON_CHAR_WIDTH; i++)
		{
			static uint8_t masks[CON_CHAR_WIDTH] = {128, 64, 32, 16, 8, 4, 2, 1, 1};
			if ((*fetch) & masks[i])
			{
				*put = CON_BASE_COLOR;
				*backPut = CON_BASE_COLOR;
			}

			put++;
			backPut++;
		};

		fetch++;
	};
};

static void conScroll()
{
	// move everything up
	size_t blobSize = conScanlineSize * (conPixelHeight - CON_CHAR_HEIGHT - CON_MARGIN);
	uint8_t *put = conBackBuffer;
	uint8_t *fetch = conBackBuffer + (conScanlineSize * (CON_CHAR_HEIGHT + CON_MARGIN));

	while (blobSize--)
	{
		*put++ = *fetch++;
	};

	// clear the end
	memset(put, 0, conScanlineSize * (CON_CHAR_HEIGHT + CON_MARGIN));

	// swap buffers
	memcpy(conFrontBuffer, conBackBuffer, conScanlineSize * conPixelHeight);

	// move cursor up
	conPosY--;
};

void conWrite(const void *data, size_t size)
{
	const char *scan = (const char*) data;

	SpinIrqState irqState = spinlockAcquire(&conLock);
	while (size--)
	{
		char c = *scan++;

		if (c == 0)
		{
			// nothing to do for NUL bytes
			continue;
		}
		else if (c == '\n')
		{
			conPosX = 0;
			conPosY++;

			if (conPosY == conHeight)
			{
				conScroll();
			};
		}
		else if (c == '\r')
		{
			conPosX = 0;
		}
		else
		{
			conRenderChar(conPosX, conPosY, c);
			conPosX++;
			if (conPosX == conWidth)
			{
				conPosX = 0;
				conPosY++;
			};

			if (conPosY == conHeight)
			{
				conScroll();
			};
		};
	};

	spinlockRelease(&conLock, irqState);
};

void conWriteString(const char *str)
{
	conWrite(str, strlen(str));
};

void conRemapFramebuffers()
{
	SpinIrqState irqState = spinlockAcquire(&conLock);

	size_t fbSize = conScanlineSize * conPixelHeight;
	size_t fbSizePages = (fbSize + 0xFFF) & ~0xFFFUL;

	uint8_t *newFrontBuffer = (uint8_t*) komAllocVirtual(fbSizePages);
	if (pagetabMapKernel(newFrontBuffer, pagetabGetPhys(conFrontBuffer), fbSizePages, PT_WRITE | PT_NOEXEC | PT_NOCACHE) != 0)
	{
		spinlockRelease(&conLock, irqState);
		panic("Failed to re-map the framebuffer!\n");
	};

	uint8_t *newBackBuffer = (uint8_t*) kmalloc(fbSize);
	if (newBackBuffer == NULL)
	{
		spinlockRelease(&conLock, irqState);
		panic("Failed to allocate a new back buffer!\n");
	};

	// copy the back buffer into the new location
	memcpy(newBackBuffer, conBackBuffer, fbSize);

	// set the buffers to the new location
	// (make sure you DON'T free the bootloader-allocated address space!)
	conFrontBuffer = newFrontBuffer;
	conBackBuffer = newBackBuffer;
	
	spinlockRelease(&conLock, irqState);
};