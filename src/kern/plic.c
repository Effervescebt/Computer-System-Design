// plic.c - RISC-V PLIC
//

#include "plic.h"
#include "console.h"

#include <stdint.h>

// COMPILE-TIME CONFIGURATION
//

// *** Note to student: you MUST use PLIC_IOBASE for all address calculations,
// as this will be used for testing!

#ifndef PLIC_IOBASE
#define PLIC_IOBASE 0x0C000000
#endif

#define PLIC_SRCCNT 0x400
#define PLIC_CTXCNT 1
#define IP_BASE 0x1000
#define IE_BASE 0x2000
#define P_THRESHOLD_BASE 0x200000
#define I_CLAIM_BASE 0x200004
#define I_COMPLETION_BASE 0x200004

// INTERNAL FUNCTION DECLARATIONS
//

// *** Note to student: the following MUST be declared extern. Do not change these
// function delcarations!

extern void plic_set_source_priority(uint32_t srcno, uint32_t level);
extern int plic_source_pending(uint32_t srcno);
extern void plic_enable_source_for_context(uint32_t ctxno, uint32_t srcno);
extern void plic_disable_source_for_context(uint32_t ctxno, uint32_t srcno);
extern void plic_set_context_threshold(uint32_t ctxno, uint32_t level);
extern uint32_t plic_claim_context_interrupt(uint32_t ctxno);
extern void plic_complete_context_interrupt(uint32_t ctxno, uint32_t srcno);

// Currently supports only single-hart operation. The low-level PLIC functions
// already understand contexts, so we only need to modify the high-level
// functions (plic_init, plic_claim, plic_complete).

// EXPORTED FUNCTION DEFINITIONS
// 

void plic_init(void) {
    int i;

    // Disable all sources by setting priority to 0, enable all sources for
    // context 0 (M mode on hart 0).

    for (i = 0; i < PLIC_SRCCNT; i++) {
        plic_set_source_priority(i, 0);
        plic_enable_source_for_context(1, i);
    }
}

extern void plic_enable_irq(int irqno, int prio) {
    trace("%s(irqno=%d,prio=%d)", __func__, irqno, prio);
    plic_set_source_priority(irqno, prio);
}

extern void plic_disable_irq(int irqno) {
    if (0 < irqno)
        plic_set_source_priority(irqno, 0);
    else
        debug("plic_disable_irq called with irqno = %d", irqno);
}

extern int plic_claim_irq(void) {
    // Hardwired context 0 (M mode on hart 0)
    trace("%s()", __func__);
    return plic_claim_context_interrupt(1);
}

extern void plic_close_irq(int irqno) {
    // Hardwired context 0 (M mode on hart 0)
    trace("%s(irqno=%d)", __func__, irqno);
    plic_complete_context_interrupt(1, irqno);
}

// INTERNAL FUNCTION DEFINITIONS
//

/*
 * @brief Set the source priority in PLIC
 * 
 * @specific
 * -> Set offset memory location given in PLIC manual
 * First entry starts at 0x0 in PLIC reserved memory,
 * thus PLIC_IOBASE in real memory.
 * Offset is srcno * 4(byte, memory occupied by each entry) + PLIC_IOBASE
 * 
 * @param srcno: interrupt source irqno or source ID
 * @param level: priority level of current interrupt source
 */

void plic_set_source_priority(uint32_t srcno, uint32_t level) {
    // FIXME your code goes here
    uint64_t offset = srcno * 4 + PLIC_IOBASE;
    //console_printf("offset at: %x\n", offset);
    *((volatile uint32_t *)offset) = level;
}

/*
 * @brief Checks pending interrupt in PLIC
 * 
 * @specific
 * -> Set offset memory location given in PLIC manual
 * First entry starts at 0x1000 in PLIC reserved memory, thus PLIC_IOBASE + IP_BASE(0x1000) in real memory.
 * Offset has term srcno/32*4. Since the pending bit for each entry is only one, we'll need 8 of them to occupy a byte in memory,
 * while memory map registers are 32bit-4byte aligned. Therefore we first do srcno/32 to get nth 4-byte memory address, and *4 to
 * get actual number of bytes in memory. If we now visit the memory location,we shall get a 32bit value indicating 32 source pending bits.
 * To set the specific bit indicated by srcno, we'll left shift 1 for srcno%32 bits(bits are stored from low to high, i.e. small to big srcid).
 * -> Left shift 1 with needed bits as explained above
 * -> Return shifted bit & bits stored in offset memory location, which results 1 if the stored pending bit is 1
 * 
 * @param srcno: interrupt source id to look up for
 * 
 * @return val: int value, 1 for source pending, 0 for not
 */

int plic_source_pending(uint32_t srcno) {
    // FIXME your code goes here
    size_t offset = IP_BASE + srcno / 32 * 4 + PLIC_IOBASE; // setting memory offset
    uint32_t bit_loc = 1 << (srcno % 32); // finding corresponding bit for srcno
    return *((volatile uint32_t *)offset) & bit_loc;
}

/*
 * @brief Enable source for specific ctxno and srcno by setting 1 for enable bit
 * 
 * @specific
 * -> Set offset memory location given in PLIC manual
 * Offset is PLIC_IOBASE + IE_BASE(0x2000) + ctxno*128(each context occupies 1024bits-128bytes) + srcno/32*4
 * For the term srcno/32*4, it's the same as previous function plic_source_pending that locates memory address where a 32-bit value with
 * the current source enable bit stored somewhere in it.
 * -> Left shift 1 with needed bits
 * Shifting srcno%32 has the same reason as done in plic_source_pending
 * -> Do shifted bit |= bits in offset memory address, corresponding enable bit will be set to 1
 * 
 * @param ctxno: the context number currently working on
 * @param srcno: the source ID of which needs to be enabled
 */

void plic_enable_source_for_context(uint32_t ctxno, uint32_t srcno) {
    // FIXME your code goes here
    size_t offset = PLIC_IOBASE + IE_BASE + ctxno * 128 + srcno / 32 * 4; // each context occupies 1024bits-128bytes in the enable bit area
    uint32_t bit_loc = 1 << (srcno % 32);
    *((volatile uint32_t *)offset) |= bit_loc; // writes 1 to required enable bit
}

/*
 * @brief Disable source for specific ctxno and srcid by setting enable bit 0
 * 
 * @specific
 * -> Set offset memory location given in PLIC manual
 * Offset is PLIC_IOBASE + IE_BASE(0x2000) + ctxno*128(each context occupies 1024bits-128bytes) + srcno/32*4
 * For the term srcno/32*4, it's the same as previous function plic_source_pending that locates memory address where a 32-bit value with
 * the current source enable bit stored somewhere in it.
 * -> Left shift 1 with needed bits
 * Shifting srcno%32 has the same reason as done in plic_source_pending
 * -> Do ~shifted bit &= bits in offset memory location, will set the corresponding enable bit to 0
 * 
 * @param ctxno: the context number currently working on
 * @param srcid: the source ID of which needs to be enabled
 */

void plic_disable_source_for_context(uint32_t ctxno, uint32_t srcid) {
    // FIXME your code goes here
    size_t offset = PLIC_IOBASE + IE_BASE + ctxno * 128 + srcid / 32 * 4; // each context occupies 1024bits-128bytes in the enable bit area
    uint32_t bit_loc = 1 << (srcid % 32);
    *((volatile uint32_t *)offset) &= ~bit_loc; // writes 0 to required enable bit
}

/*
 * @brief Set threshold for specified context
 *
 * @specific
 * -> Set offset memory location given in PLIC manual
 * Offset is PLIC_IOBASE + P_THRESHOLD_BASE(0X200000) + ctxno*4096(that adjacent context threshold memory locations are 4096 bytes away)
 * -> Store priority threshold (input uint32_t level) to the offset memory location
 * 
 * @param ctxno: the context number currently working on
 * @param level: the priority threshold to be set
 */

void plic_set_context_threshold(uint32_t ctxno, uint32_t level) {
    // FIXME your code goes here
    size_t offset = P_THRESHOLD_BASE + ctxno * 4096 + PLIC_IOBASE; // 4096 is the byte number to jump from one threshold location to the next
    *((volatile uint32_t *)offset) = level;
}

/*
 * @brief Claim the interrupt for specific context to handle
 *
 * @Documentation This function reads from the claim register and returns the interrupt ID of the highest-priority pending interrupt. It returns 0 if no interrupts are pending.
 *
 * @specific
 * -> Set offset memory location given in PLIC manual
 * Offset at PLIC_IOBASE + I_CLAIM_BASE(0x200004) + ctxno*4096(that adjacent claim process memory locations are 4096 bytes away)
 * -> Return srcno stored in the offset address
 * This srcno is the interrupt source with highest priority
 * This function simply performs a read from that offset address
 * 
 * @param ctxno: the context number currently working on
 * @return val: the highest priority source's ID
 */

uint32_t plic_claim_context_interrupt(uint32_t ctxno) {
    // FIXME your code goes here
    size_t offset = PLIC_IOBASE + I_CLAIM_BASE + ctxno * 4096; // (adjacent claim process memory locations are 4096 bytes away)
    
    return *((volatile uint32_t *)offset);
}

/*
 * @brief Writes back the srcno being handled by specific context
 *
 * @Documentation This function writes the interrupt source number back to the claim register, notifying the PLIC that the interrupt has been serviced.
 *
 * @specific
 * -> Set the offset memory location given in PLIC context
 * Offset is the same as plic_claim_context_interrupt
 * There's another base called I_COMPLETION_BASE(same as I_CLAIM_BASE), intended to be used in this function, yet not needed
 * -> Stores the srcno into the offset location specified by ctxno
 * This will tell PLIC that srcno has been handled.
 * 
 * @param ctxno: the context number currently working on
 * @param srcno: the source ID currently being handled
 */

void plic_complete_context_interrupt(uint32_t ctxno, uint32_t srcno) {
    // FIXME your code goes here
    size_t hart_offset = I_CLAIM_BASE + ctxno * 4096 + PLIC_IOBASE; // (adjacent completion memory locations are 4096 bytes away)
    *((volatile uint32_t *)hart_offset) = srcno;
}