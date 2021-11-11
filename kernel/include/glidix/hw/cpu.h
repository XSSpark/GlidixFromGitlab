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

#ifndef __glidix_hw_cpu_h
#define	__glidix_hw_cpu_h

#include <glidix/util/common.h>
#include <glidix/thread/sched.h>
#include <glidix/hw/tss.h>

/**
 * Size of the lowmem mapping when setting up APs.
 */
#define	CPU_LOWMEM_SIZE					(1024 * 1024)

/**
 * Offsets to positions within lowmem.
 */
#define	CPU_LOWMEM_TRAM_CODE				0xA000
#define	CPU_LOWMEM_TRAM_DATA				0xB000
#define	CPU_LOWMEM_PML4					0xC000
#define	CPU_LOWMEM_GDT					0xD000

/**
 * Size of the idle stack.
 */
#define	CPU_IDLE_STACK_SIZE				(64 * 1024)

/**
 * Size of the startup stack (used by the startup thread on each CPU).
 */
#define	CPU_STARTUP_STACK_SIZE				(64 * 1024)

/**
 * Max number of CPUs.
 */
#define	CPU_MAX						128

/**
 * CPU message types.
 */
#define	CPU_MSG_INVLPG					1		/* invalidate page */
#define	CPU_MSG_INVLPG_TABLE				2		/* invalidate the whole page table */
#define	CPU_MSG_PROC_SIGNAL				3		/* process received signal */
#define	CPU_MSG_THREAD_SIGNAL				4		/* thread received signal */

/**
 * Represents a message for the CPU.
 */
typedef struct CPUMessage_ CPUMessage;
struct CPUMessage_
{
	/**
	 * Next message.
	 */
	CPUMessage *next;
	
	/**
	 * The message type (`CPU_MSG_*`).
	 */
	int msgType;

	/**
	 * Message response, if applicable.
	 */
	volatile int msgResp;

	/**
	 * Message parameter if applicable.
	 */
	void *param;

	/**
	 * Target CPU sets this to acknowledge that the message has been
	 * processed and `msgResp` is set.
	 */
	volatile int ack;

	/**
	 * The thread waiting for this message to be processed.
	 */
	Thread *waiter;
};

/**
 * Represents a CPU. Some of the fields here must have specific offsets, as they are
 * accessed from assembly. These are marked with a comment specifying the offset.
 */
typedef struct CPU_ CPU;
struct CPU_
{
	/**
	 * Points back to itself.
	 */
	CPU *self;							// 0x00

	/**
	 * The current thread running on this CPU.
	 */
	Thread *currentThread;						// 0x08

	/**
	 * Kernel stack pointer when entering a syscall.
	 */
	void* syscallStackPointer;					// 0x10

	/**
	 * "Syscall save slot", this is used in `syscall.asm` to save a temporary
	 * value.
	 */
	uint64_t syscallSaveSlot;					// 0x18

	// --- END OF ASSEMBLY-USEABLE AREA ---
	// --- PLEASE KEEP THIS ALIGNED AT 16-BYTE BOUNDARY (CURRENTLY AT
	// 0x20) SO THAT THE STACKS BELOW ARE ALIGNED! ---

	/**
	 * Space reserved for the idle thread stack.
	 */
	char idleStack[CPU_IDLE_STACK_SIZE];

	/**
	 * Space reserved for the stratup thread stack.
	 */
	char startupStack[CPU_STARTUP_STACK_SIZE];

	/**
	 * The TSS for this CPU.
	 */
	TSS tss;

	/**
	 * The 'idle thread' for this CPU.
	 */
	Thread idleThread;

	/**
	 * This CPU's APIC ID.
	 */
	uint8_t apicID;

	/**
	 * GDT pointer for APs.
	 */
	struct
	{
		uint16_t limit;
		void *base;
	} PACKED gdtPtr;

	/**
	 * GDT for an AP.
	 */
	uint8_t gdt[64];

	/**
	 * Physical address of the 'kernel page table', to be used for the idle
	 * thread and all other kernel threads.
	 */
	uint64_t kernelCR3;

	/**
	 * The current CR3 set on this CPU. The CPU will set this field just before
	 * it actually switches to this CR3, and this is used so we know who to send
	 * the page table invalidation IPI to.
	 */
	volatile uint64_t currentCR3;

	/**
	 * Spinlock protecting the message queue.
	 */
	Spinlock msgLock;

	/**
	 * Pending message list.
	 */
	CPUMessage *msg;
};

/**
 * Data area which the trampoline code and the main kernel use to communicate.
 */
typedef struct
{
	/**
	 * Flags used by AP and BSP during startup. flagAP2BSP is set by the AP,
	 * to communicate to the BSP it has booted. Then, to avoid the BSP rebooting
	 * the AP due to a race condition, flagBSP2AP is then set by the BSP to tell
	 * the AP that it can continue.
	 */
	volatile int flagAP2BSP;		// 0x0000
	volatile int flagBSP2AP;		// 0x0004

	/**
	 * Flag set by the AP when it's done initializing the trampoline data can be
	 * released.
	 */
	volatile int flagAPDone;		// 0x0008

	// 0x000C
	// (padded to 0x0010)
	
	/**
	 * Pointer to the 64-bit `GDTPointer`.
	 */
	void *realGDTPtr;			// 0x0010

	/**
	 * Temp GDT pointer.
	 */
	struct
	{
		uint16_t limit;
		uint64_t base;
	} PACKED tempGDT;			// 0x0018

	/**
	 * Physical address of this AP's initial PML4.
	 */
	uint64_t pml4Phys;			// 0x0028

	/**
	 * Pointer to the idtPtr.
	 */
	void *idtPtrPtr;			// 0x0030

	/**
	 * Initial stack pointer.
	 */
	uint64_t initRSP;			// 0x0038

	// 0x0040
} TrampolineData;

/**
 * Initialize the calling CPU's structures. The CPU has index `index` allocated
 * in the CPU array.
 */
void cpuInitSelf(int index);

/**
 * Get the CPU descriptor for the calling CPU.
 */
CPU* cpuGetCurrent();

/**
 * Report that a CPU with the specified APIC ID was detected, and should be enabled
 * when `cpuStartAPs()` is called.
 */
void cpuRegister(uint8_t apicID);

/**
 * Start up the application processors.
 */
void cpuStartAPs();

/**
 * Get the number of CPUs.
 */
int cpuGetCount();

/**
 * Send the `I_IPI_WAKE` interrupt to the specified CPU index.
 * 
 * NOTE: Only call this when interrupts are disabled!
 */
void cpuWake(int index);

/**
 * Return the index of the current CPU.
 */
int cpuGetMyIndex();

/**
 * Get the CPU with the specified index.
 */
CPU* cpuGetIndex(int index);

/**
 * Send a message to the specified CPU. Waits until the CPU acknowledges the message,
 * and returns a response.
 */
int cpuSendMessage(int index, int msgType, void *param);

/**
 * This is called when the `I_IPI_MESSAGE` interrupt is received. Process all messages in
 * our message list.
 */
void cpuProcessMessages();

/**
 * Invalidate the TLB entry for the specified pointer in the specified CR3, among all CPUs.
 */
void cpuInvalidatePage(uint64_t cr3, void *ptr);

/**
 * Tell other CPUs that the process using the specified CR3 received a signal, and so someone
 * should dispatch it.
 */
void cpuInformProcSignalled(Process *proc);

/**
 * Tell other CPUs that the specified thread received a signal, and so someone should dispatch
 * it.
 */
void cpuInformThreadSignalled(Thread *thread);

#endif