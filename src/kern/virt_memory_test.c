#include "console.h"
#include "thread.h"
#include "device.h"
#include "uart.h"
#include "timer.h"
#include "intr.h"
#include "memory.h"
#include "heap.h"
#include "virtio.h"
#include "halt.h"
#include "elf.h"
#include "fs.h"
#include "string.h"
#include "process.h"
#include "config.h"


void main(void) {
    console_init();
    memory_init();
    intr_init();
    devmgr_init();
    thread_init();
    procmgr_init();

    kprintf("-------------------------- Testing Basic Paging --------------------------\n");

    uintptr_t usr_end = 0xD0000000;
    memory_alloc_and_map_page((uintptr_t)usr_end, 0);
    struct pte* mapped_pte = walk_pt(active_space_root(), usr_end, 0);
    kprintf("mapping vma %x to pma %x in pte %x\n", usr_end, mapped_pte->ppn << 12, mapped_pte);
    uintptr_t usr_stack = 0xCfffffff;
    memory_alloc_and_map_page((uintptr_t)usr_stack, PTE_W | PTE_R);
    mapped_pte = walk_pt(active_space_root(), usr_stack, 0);
    kprintf("mapping vma %x to pma %x in pte %x\n", usr_stack, (mapped_pte->ppn << 12) | (usr_stack & 0xFFF), mapped_pte);
    usr_stack = 0xCffffff2;
    mapped_pte = walk_pt(active_space_root(), usr_stack, CREATE_PTE);
    kprintf("mapping vma %x to pma %x in pte %x\n", usr_stack, (mapped_pte->ppn << 12) | (usr_stack & 0xFFF), mapped_pte);
    kprintf("-------------------------- Testing Accessing Mapped Page --------------------------\n");
    *((volatile uintptr_t*)usr_stack) = 1000;
    kprintf("successful mapping\n");
    kprintf("------------------------ Testing Accessing Page Without W Permission ------------------------\n");
    memory_set_page_flags((void *)usr_stack, 0);
    *((volatile uintptr_t*)usr_stack) = 1000;
    kprintf("------------------------ Testing Accessing Page Without R Permission ------------------------\n");
    int collected_data = *((volatile uintptr_t*)usr_stack);
    if (collected_data) {}
}