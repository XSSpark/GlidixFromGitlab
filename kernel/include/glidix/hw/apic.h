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

#ifndef __glidix_hw_apic_h
#define	__glidix_hw_apic_h

#include <glidix/util/common.h>

/**
 * Physical base address of the APIC register space.
 */
#define	APIC_PHYS_BASE			0xFEE00000

#define	APICRSV				const uint32_t ALIGN(16)
#define	APICRO				const volatile uint32_t ALIGN(16)
#define	APICWO				volatile uint32_t ALIGN(16)
#define	APICRW				volatile uint32_t ALIGN(16)

/**
 * APIC base address register.
 */
#define	MSR_APIC_BASE			0x1B

/**
 * Flag in the `MSR_APIC_BASE` to enable the local APIC.
 */
#define	APIC_BASE_ENABLE		(1 << 11)

/**
 * APIC Interrupt Command Register (ICR) destination modes.
 */
#define	APIC_ICR_DESTMODE_NORMAL	(0 << 8)
#define	APIC_ICR_DESTMODE_LOWPRIO	(1 << 8)
#define	APIC_ICR_DESTMODE_SMI		(2 << 8)
#define	APIC_ICR_DESTMODE_NMI		(4 << 8)
#define	APIC_ICR_DESTMODE_INIT		(5 << 8)
#define	APIC_ICR_DESTMODE_SIPI		(6 << 8)

/**
 * Are we doing an INIT level de-assert?
 */
#define	APIC_ICR_INITDEAS_NO		(1 << 14)
#define	APIC_ICR_INITDEAS_YES		(1 << 5)

/**
 * Delivery status bit in the ICR.
 */
#define	APIC_ICR_PENDING		(1 << 12)

/**
 * Keep this consistent with the layout in the Intel manual.
 */
typedef struct
{
	APICRSV				_rsv1;
	APICRSV				_rsv2;
	APICRO				id;
	APICRO				version;
	APICRSV				_rsv3;
	APICRSV				_rsv4;
	APICRSV				_rsv5;
	APICRSV				_rsv6;
	APICRW				tpr;
	APICRO				apr;
	APICRO				ppr;
	APICWO				eoi;
	APICRO				rrd;
	APICRW				ldr;
	APICRW				dfr;
	APICRW				sivr;
	APICRO				isr0;
	APICRO				isr1;
	APICRO				isr2;
	APICRO				isr3;
	APICRO				isr4;
	APICRO				isr5;
	APICRO				isr6;
	APICRO				isr7;
	APICRO				tmr0;
	APICRO				tmr1;
	APICRO				tmr2;
	APICRO				tmr3;
	APICRO				tmr4;
	APICRO				tmr5;
	APICRO				tmr6;
	APICRO				tmr7;
	APICRO				irr0;
	APICRO				irr1;
	APICRO				irr2;
	APICRO				irr3;
	APICRO				irr4;
	APICRO				irr5;
	APICRO				irr6;
	APICRO				irr7;
	APICRO				error;
	APICRO				_rsv7;
	APICRO				_rsv8;
	APICRO				_rsv9;
	APICRO				_rsv10;
	APICRO				_rsv11;
	APICRO				_rsv12;
	APICRW				lvtCMCI;
	APICRW				icr;
	APICRW				icrDestApicID;
	APICRW				lvtTimer;
	APICRW				lvtThermalSensor;
	APICRW				lvtPerformanceMonitor;
	APICRW				lvtLINT0;
	APICRW				lvtLINT1;
	APICRW				lvtError;
	APICRW				timerInitCount;
	APICRO				timerCurrentCount;
	APICRSV				_rsv13;
	APICRSV				_rsv14;
	APICRSV				_rsv15;
	APICRSV				_rsv16;
	APICRW				timerDivide;
	APICRSV				_rsv17;
} APICRegisterSpace;

/**
 * The APIC register space. This symbol is defined in `kernel.ld`, such that the bootloader
 * maps it to the physical address of the APIC register space.
 */
extern volatile APICRegisterSpace apic;

#endif