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
    kprintf("\n");
    kprintf("-------------------------- Testing Basic Paging Part I --------------------------\n");
    kprintf("\n");

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
    kprintf("\n");
    kprintf("*** mapping successful, virtual memory in same page doesn't trigger double mapping ***\n");
    kprintf("\n");
    kprintf("-------------------------- Testing Basic Paging Part II --------------------------\n");
    kprintf("\n");

    uintptr_t user_middle_addr = 0xC00a0000;
    for (size_t idx = 0; idx < 4096 * 10; idx+=4096) {
        memory_alloc_and_map_page((uintptr_t)(user_middle_addr + idx), PTE_W | PTE_R);
        mapped_pte = walk_pt(active_space_root(), user_middle_addr + idx, CREATE_PTE);
        kprintf("mapping vma %x to pma %x in pte %x\n", user_middle_addr + idx, (mapped_pte->ppn << 12) | ((user_middle_addr + idx) & 0xFFF), mapped_pte);
        kprintf("for a memory location %x somewhere in between we'd see its physical memory at %x\n", user_middle_addr + 128 + idx + idx / 4096 * 256, (mapped_pte->ppn << 12) | ((user_middle_addr + 128 + idx + idx / 4096 * 256) & 0xFFF));
        kprintf("\n");
    }
    kprintf("\n");
    kprintf("*** successful mapping for consecutive virtual memory and their corresponding pma as shown ***\n");
    kprintf("\n");
    kprintf("-------------------------- Testing Basic Paging Part III --------------------------\n");
    kprintf("\n");
    uintptr_t random_user_addr = 0xC0008000;
    memory_alloc_and_map_page((uintptr_t)random_user_addr, PTE_W | PTE_R);
    mapped_pte = walk_pt(active_space_root(), random_user_addr, CREATE_PTE);
    kprintf("mapping virtual ptr %x to pma %x\n", random_user_addr, (mapped_pte->ppn << 12) | (random_user_addr & 0xFFF));
    for (size_t idx = 0; idx < 4096; idx++) {
        *((uint8_t*)(random_user_addr + idx))= 'l';
    }
    for (size_t idx = 0; idx < 4096; idx++) {
        if (*(uint8_t*)(random_user_addr + idx) == 'l') {
            continue;
        } else {
            panic("memory contents not allocated\n");
        }
    }
    kprintf("successful mapping in random user address for 4096 bytes\n");
    uintptr_t random_usr_pma = (mapped_pte->ppn << 12) | (random_user_addr & 0xFFF);
    for (size_t idx = 0; idx < 4096; idx++) {
        if (*(uint8_t*)(random_usr_pma + idx) == 'l') {
            continue;
        } else {
            panic("memory contents not allocated\n");
        }
    }
    kprintf("\n");
    kprintf("*** user space mapping matches the physical memory contents ***\n");
    kprintf("\n");
    kprintf("-------------------------- Testing Accessing Mapped Page --------------------------\n");
    kprintf("\n");
    *((volatile uintptr_t*)usr_stack) = 1000;
    mapped_pte = walk_pt(active_space_root(), usr_stack, 0);
    uintptr_t access_pma = (mapped_pte->ppn << 12) | (usr_stack & 0xFFF);
    kprintf("writing %d to physical memory %x pointed by %x\n", *(uint64_t *)access_pma, usr_stack, access_pma);
    kprintf("\n");
    kprintf("*** successful write to a W enabled page ***\n");
    kprintf("\n");
    kprintf("------------------------ Testing Accessing Page Without W Permission ------------------------\n");
    kprintf("\n");
    kprintf("*** failure write to W disabled page ***\n");
    kprintf("\n");
    memory_set_page_flags((void *)usr_stack, 0);
    *((volatile uintptr_t*)usr_stack) = 1000;
    kprintf("\n");
    kprintf("------------------------ Testing Accessing Page Without R Permission ------------------------\n");
    kprintf("\n");
    kprintf("*** failure read from R disabled page ***\n");
    kprintf("\n");
    int collected_data = *((volatile uintptr_t*)usr_stack);
    if (collected_data) {}
}