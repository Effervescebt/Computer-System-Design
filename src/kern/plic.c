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
        plic_enable_source_for_context(0, i);
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
    return plic_claim_context_interrupt(0);
}

extern void plic_close_irq(int irqno) {
    // Hardwired context 0 (M mode on hart 0)
    trace("%s(irqno=%d)", __func__, irqno);
    plic_complete_context_interrupt(0, irqno);
}

// INTERNAL FUNCTION DEFINITIONS
//

/* This function sets the priority level for a specific interrupt source.
* It modifies the priority array, where each entry corresponds to a specific interrupt source.
*
* Arguments: srcno-source number    level-priority level
* Return: void
*/
void plic_set_source_priority(uint32_t srcno, uint32_t level) {
    // FIXME your code goes here

    // 32/8=4 is the offset for each priority regesiter. 
    uint64_t addr = PLIC_IOBASE + 4 * srcno;
    *((volatile uint32_t*) addr) = level;
}

/* This function checks if an interrupt source is pending by inspecting the pending array.
* The pending bit is determined by checking the bit corresponding to srcno in the pending array.
*
* Arguments: srcno-source number
* Return: 1 if the interrupt is pending, 0 otherwise
*/
int plic_source_pending(uint32_t srcno) {
    // FIXME your code goes here

    // 32/8=4 is the offset for each pending regesiter. 
    // The pending registers start from base + 0x1000. 
    uint64_t addr = PLIC_IOBASE + 0x1000 + 4 * (srcno / 32);
    int bit = srcno % 32;   // The exact bit position in the register. 
    if (*((volatile uint32_t*) addr) & (1 << bit)) return 1;   // Check if the pending bit is 1. 
    else    return 0;
}

 
/* This function enables a specific interrupt source for a given context.
* It sets the appropriate bit in the enable array. It calculates the index based on the source number
* and context, and sets the corresponding bit for the source.
*
* Arguments: ctxno-contex number    srcno-source number
* Return: void
*/
void plic_enable_source_for_context(uint32_t ctxno, uint32_t srcno) {
    // FIXME your code goes here

    // 32/8=4 is the offset for each enables regesiter. 
    // The enables registers start from base + 0x2000. Each context is 0x80 long. 
    uint64_t addr = PLIC_IOBASE + 0x2000 + 0x80 * ctxno + 4 * (srcno / 32);
    int bit = srcno % 32;   // The exact bit position in the register.   
    *((volatile uint32_t*) addr) |= (1 << bit); // Set the enable bit to 1. 
}

/* This function disables a specific interrupt source for a given context.
* It clears the appropriate bit in the enable array. Similar to plic enable source for context,
*
* it calculates the correct bit to clear for the given context and source.
* Arguments: ctxno-contex number    srcid-source id
* Return: void
*/
void plic_disable_source_for_context(uint32_t ctxno, uint32_t srcid) {
    // FIXME your code goes here

    // 32/8=4 is the offset for each enables regesiter. 
    // The enables registers start from base + 0x2000. Each context is 0x80 long. 
    uint64_t addr = PLIC_IOBASE + 0x2000 + 0x80 * ctxno + 4 * (srcid / 32);
    int bit = srcid % 32;   // The exact bit position in the register.   
    *((volatile uint32_t*) addr) &= ~(1 << bit); // Set the enable bit to 0. 
}

/* This function sets the interrupt priority threshold for a specific context.
* Interrupts with a priority lower than the threshold will not be handled by the context. 
*
* Arguments: ctxno-contex number    level-threshold level
* Return: void
*/
void plic_set_context_threshold(uint32_t ctxno, uint32_t level) {
    // FIXME your code goes here

    // 32/8=4 is the offset for each threshold regesiter. 
    // The threshold registers start from base + 0x200000. Each context is 0x1000 long. 
    uint64_t addr = PLIC_IOBASE + 0x200000 + 0x1000 * ctxno;
    *((volatile uint32_t*) addr) = level;
}

/* This function claims an interrupt for a given context.
* It reads from the claim register and returns the interrupt ID of the highest-priority pending interrupt. 
*
* Arguments: ctxno-contex number
* Return: the interrupt ID or 0 if there is no interrupts. 
*/
uint32_t plic_claim_context_interrupt(uint32_t ctxno) {
    // FIXME your code goes here

    // 32/8=4 is the offset for each claim regesiter. 
    // The claim registers start from base + 0x200004. Each context is 0x1000 long. 
    uint64_t addr = PLIC_IOBASE + 0x200004 + 0x1000 * ctxno;
    return *((volatile uint32_t*) addr);  // claim return 0 if there is no pending interrupt. 
}

/* This function completes the handling of an interrupt for a given context.
* It writes the interrupt source number back to the claim register, 
* notifying the PLIC that the interrupt has been serviced.
*
* Arguments: ctxno-contex number    srcno-source number
* Return: void
*/
void plic_complete_context_interrupt(uint32_t ctxno, uint32_t srcno) {
    // FIXME your code goes here

    // 32/8=4 is the offset for each claim regesiter. 
    // The claim registers start from base + 0x200004. Each context is 0x1000 long. 
    uint64_t addr = PLIC_IOBASE + 0x200004 + 0x1000 * ctxno;
    *((volatile uint32_t*) addr) = srcno;
}