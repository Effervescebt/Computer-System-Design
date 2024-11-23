// memory.c - Memory management
//

#ifndef TRACE
#ifdef MEMORY_TRACE
#define TRACE
#endif
#endif

#ifndef DEBUG
#ifdef MEMORY_DEBUG
#define DEBUG
#endif
#endif

#include "config.h"

#include "memory.h"
#include "console.h"
#include "halt.h"
#include "heap.h"
#include "csr.h"
#include "string.h"
#include "error.h"
#include "thread.h"
#include "process.h"

#include <stdint.h>

// EXPORTED VARIABLE DEFINITIONS
//

char memory_initialized = 0;
uintptr_t main_mtag;

// IMPORTED VARIABLE DECLARATIONS
//

// The following are provided by the linker (kernel.ld)

extern char _kimg_start[];
extern char _kimg_text_start[];
extern char _kimg_text_end[];
extern char _kimg_rodata_start[];
extern char _kimg_rodata_end[];
extern char _kimg_data_start[];
extern char _kimg_data_end[];
extern char _kimg_end[];

// INTERNAL TYPE DEFINITIONS
//

union linked_page {
    union linked_page * next;
    char padding[PAGE_SIZE];
};



// INTERNAL MACRO DEFINITIONS
//

#define VPN2(vma) (((vma) >> (9+9+12)) & 0x1FF)
#define VPN1(vma) (((vma) >> (9+12)) & 0x1FF)
#define VPN0(vma) (((vma) >> 12) & 0x1FF)
#define MIN(a,b) (((a)<(b))?(a):(b))

// INTERNAL FUNCTION DECLARATIONS
//

static inline int wellformed_vma(uintptr_t vma);
static inline int wellformed_vptr(const void * vp);
static inline int aligned_addr(uintptr_t vma, size_t blksz);
static inline int aligned_ptr(const void * p, size_t blksz);
static inline int aligned_size(size_t size, size_t blksz);

static inline void * pagenum_to_pageptr(uintptr_t n);
static inline uintptr_t pageptr_to_pagenum(const void * p);

static inline void * round_up_ptr(void * p, size_t blksz);
static inline uintptr_t round_up_addr(uintptr_t addr, size_t blksz);
static inline size_t round_up_size(size_t n, size_t blksz);
static inline void * round_down_ptr(void * p, size_t blksz);
static inline size_t round_down_size(size_t n, size_t blksz);
static inline uintptr_t round_down_addr(uintptr_t addr, size_t blksz);

static inline struct pte leaf_pte (
    const void * pptr, uint_fast8_t rwxug_flags);
static inline struct pte ptab_pte (
    const struct pte * ptab, uint_fast8_t g_flag);
static inline struct pte null_pte(void);

static inline void sfence_vma(void);

// INTERNAL GLOBAL VARIABLES
//

static union linked_page * free_list;

static struct pte main_pt2[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));
static struct pte main_pt1_0x80000[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));
static struct pte main_pt0_0x80000[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));

// EXPORTED VARIABLE DEFINITIONS
//

// EXPORTED FUNCTION DEFINITIONS
// 

struct pte* walk_pt(struct pte* root, uintptr_t vma, int create) {
    if (create != 0) {
        if ((root[VPN2(vma)].flags & PTE_V) == 0) {
            struct pte* new_pt1 = kmalloc(PAGE_SIZE);
            root[VPN2(vma)] = ptab_pte(new_pt1, 0); // if create enabled, creates a level 1 sub-directory
            sfence_vma();
        }
    }
    // Now we may access the ppn in the pte VPN2 points to
    uintptr_t pt1_ppn = root[VPN2(vma)].ppn; 
    // Extract the level 1 sub-directory VPN2 indicates
    struct pte* subdirectory_pt1 = pagenum_to_pageptr(pt1_ppn);

    if (create != 0) {
        if ((subdirectory_pt1[VPN1(vma)].flags & PTE_V) == 0) {
            struct pte* new_pt0 = kmalloc(PAGE_SIZE);
            subdirectory_pt1[VPN1(vma)] = ptab_pte(new_pt0, 0); // creates a leaf directory
            sfence_vma();
        }
    }
    // Now we may access the ppn in the pte VPN1 points to
    uintptr_t pt0_ppn = subdirectory_pt1[VPN1(vma)].ppn;
    // Extract the leaf directory VPN1 indicates
    struct pte* leafdirectory_pt0 = pagenum_to_pageptr(pt0_ppn);

    // Find the pte this vma points to
    struct pte* virtmem_pte_ptr = &leafdirectory_pt0[VPN0(vma)];
    if (create != 0) {
        leafdirectory_pt0[VPN0(vma)].flags |= PTE_V;
    }
    return virtmem_pte_ptr;
}

void memory_init(void) {
    const void * const text_start = _kimg_text_start;
    const void * const text_end = _kimg_text_end;
    const void * const rodata_start = _kimg_rodata_start;
    const void * const rodata_end = _kimg_rodata_end;
    const void * const data_start = _kimg_data_start;
    union linked_page * page;
    void * heap_start;
    void * heap_end;
    size_t page_cnt;
    uintptr_t pma;
    const void * pp;

    trace("%s()", __func__);

    assert (RAM_START == _kimg_start);

    kprintf("           RAM: [%p,%p): %zu MB\n",
        RAM_START, RAM_END, RAM_SIZE / 1024 / 1024);
    kprintf("  Kernel image: [%p,%p)\n", _kimg_start, _kimg_end);

    // Kernel must fit inside 2MB megapage (one level 1 PTE)
    
    if (MEGA_SIZE < _kimg_end - _kimg_start)
        panic("Kernel too large");

    // Initialize main page table with the following direct mapping:
    // 
    //         0 to RAM_START:           RW gigapages (MMIO region)
    // RAM_START to _kimg_end:           RX/R/RW pages based on kernel image
    // _kimg_end to RAM_START+MEGA_SIZE: RW pages (heap and free page pool)
    // RAM_START+MEGA_SIZE to RAM_END:   RW megapages (free page pool)
    //
    // RAM_START = 0x80000000
    // MEGA_SIZE = 2 MB
    // GIGA_SIZE = 1 GB
    
    // Identity mapping of two gigabytes (as two gigapage mappings)
    for (pma = 0; pma < RAM_START_PMA; pma += GIGA_SIZE)
        main_pt2[VPN2(pma)] = leaf_pte((void*)pma, PTE_R | PTE_W | PTE_G);
    
    // Third gigarange has a second-level page table
    main_pt2[VPN2(RAM_START_PMA)] = ptab_pte(main_pt1_0x80000, PTE_G);

    // First physical megarange of RAM is mapped as individual pages with
    // permissions based on kernel image region.

    main_pt1_0x80000[VPN1(RAM_START_PMA)] =
        ptab_pte(main_pt0_0x80000, PTE_G);

    for (pp = text_start; pp < text_end; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_X | PTE_G);
    }

    for (pp = rodata_start; pp < rodata_end; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_G);
    }

    for (pp = data_start; pp < RAM_START + MEGA_SIZE; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Remaining RAM mapped in 2MB megapages

    for (pp = RAM_START + MEGA_SIZE; pp < RAM_END; pp += MEGA_SIZE) {
        main_pt1_0x80000[VPN1((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Enable paging. This part always makes me nervous.

    main_mtag =  // Sv39
        ((uintptr_t)RISCV_SATP_MODE_Sv39 << RISCV_SATP_MODE_shift) |
        pageptr_to_pagenum(main_pt2);
    
    csrw_satp(main_mtag);
    sfence_vma();

    // Give the memory between the end of the kernel image and the next page
    // boundary to the heap allocator, but make sure it is at least
    // HEAP_INIT_MIN bytes.

    heap_start = _kimg_end;
    heap_end = round_up_ptr(heap_start, PAGE_SIZE);
    if (heap_end - heap_start < HEAP_INIT_MIN) {
        heap_end += round_up_size (
            HEAP_INIT_MIN - (heap_end - heap_start), PAGE_SIZE);
    }

    if (RAM_END < heap_end)
        panic("Not enough memory");
    
    // Initialize heap memory manager

    heap_init(heap_start, heap_end);

    kprintf("Heap allocator: [%p,%p): %zu KB free\n",
        heap_start, heap_end, (heap_end - heap_start) / 1024);

    free_list = heap_end; // heap_end is page aligned
    page_cnt = (RAM_END - heap_end) / PAGE_SIZE;

    kprintf("Page allocator: [%p,%p): %lu pages free\n",
        free_list, RAM_END, page_cnt);

    // Put free pages on the free page list
    // TODO: FIXME implement this (must work with your implementation of
    // memory_alloc_page and memory_free_page).

    // allocate all free pages, each with size PAGE_SIZE, from the end of head_end to RAM_END

    // page count provided above
    union linked_page * aux_ptr = free_list; 
    for (size_t free_pg_idx = PAGE_SIZE; free_pg_idx < page_cnt * PAGE_SIZE; free_pg_idx += PAGE_SIZE) {
        // free_list already pointing to heap_end, so start with free_list->next
        aux_ptr->next = (union linked_page *)(free_pg_idx + heap_end);
        aux_ptr = aux_ptr->next;
    }
    
    // Allow supervisor to access user memory. We could be more precise by only
    // enabling it when we are accessing user memory, and disable it at other
    // times to catch bugs.

    csrs_sstatus(RISCV_SSTATUS_SUM);

    memory_initialized = 1;
}

void memory_space_reclaim(void) {
    uintptr_t prev_mtag = memory_space_switch(main_mtag);
    union linked_page* tail_free_list = free_list;
    if (tail_free_list != NULL) {
        while (tail_free_list->next != NULL) {
            tail_free_list = tail_free_list->next;
        }
    }
    struct pte* prev_pt2 = pagenum_to_pageptr(prev_mtag);

    // Loops through all three directories for every PTE without global tag G
    for (size_t pt2_idx = 0; pt2_idx < PTE_CNT; pt2_idx++) {
        if (prev_pt2[pt2_idx].flags & PTE_V != 0  && prev_pt2[pt2_idx].ppn != NULL) {
            struct pte* subdirectory_pt1 = pagenum_to_pageptr(prev_pt2[pt2_idx].ppn);
            for (size_t pt1_idx = 0; pt1_idx < PTE_CNT; pt1_idx++) {
                if (subdirectory_pt1[pt1_idx].flags & PTE_V != 0 && subdirectory_pt1[pt1_idx].ppn != NULL) {
                    struct pte* leafdirectory_pt0 = pagenum_to_pageptr(subdirectory_pt1[pt1_idx].ppn);
                    for (size_t pt0_idx = 0; pt0_idx < PTE_CNT; pt0_idx++) {
                        if (leafdirectory_pt0[pt0_idx].flags & PTE_V != 0 && leafdirectory_pt0[pt0_idx].ppn != NULL) {
                            if (leafdirectory_pt0[pt0_idx].flags & PTE_G == 0) {
                                if (tail_free_list == NULL) {
                                    tail_free_list = pagenum_to_pageptr(leafdirectory_pt0[pt0_idx].ppn);
                                    free_list = tail_free_list;
                                } else {
                                    tail_free_list->next = pagenum_to_pageptr(leafdirectory_pt0[pt0_idx].ppn);
                                    tail_free_list = tail_free_list->next;
                                }
                                leafdirectory_pt0[pt0_idx].ppn &= 0;
                                leafdirectory_pt0[pt0_idx].flags &= 0;
                                sfence_vma();
                            }
                        }
                    }
                    if (subdirectory_pt1[pt1_idx].flags & PTE_G == 0) {
                        memory_free_page(leafdirectory_pt0);
                        sfence_vma();
                    }
                }
            }
            if (prev_pt2[pt2_idx].flags & PTE_G == 0) {
                memory_free_page(subdirectory_pt1);
                sfence_vma();
            }
        }
    }
    memory_free_page(prev_pt2);
    sfence_vma();
}

void * memory_alloc_page(void) {
    // Extract the head of free_list thus make it the pma to allocate
    if (free_list == NULL) {
        panic("No Available Free Space\n");
    }
    union linked_page * physical_mem = free_list;
    free_list = free_list->next;

    return physical_mem;
}

void memory_free_page(void * pp) {
    union linked_page * tail_free_list = free_list;
    if (tail_free_list != NULL) {
        while (tail_free_list->next != NULL) {
            if (tail_free_list == pp) {
                panic("Address Not Previously Allocated\n");
            }
            tail_free_list = tail_free_list->next;
        }
    }
    if (tail_free_list == NULL) {
        tail_free_list = (union linked_page *)pp;
        free_list = tail_free_list;
    } else {
        tail_free_list->next = (union linked_page *)pp;
        tail_free_list->next->next = NULL;
    }
    struct pte* free_pte = (struct pte*)walk_pt(active_space_root(), pp, 0);
    free_pte->flags &= 0;
    free_pte->ppn &= 0;
    sfence_vma();
}

void * memory_alloc_and_map_page (uintptr_t vma, uint_fast8_t rwxug_flags) {
    uintptr_t newly_allocated = (uintptr_t *)(memory_alloc_page());

    struct pte* dest_pte = (struct pte*)walk_pt(active_space_root(), vma, CREATE_PTE);
    dest_pte->flags |= rwxug_flags | PTE_A | PTE_D | PTE_V;
    dest_pte->ppn = pageptr_to_pagenum(newly_allocated);
    sfence_vma();
    return vma;
}

void * memory_alloc_and_map_range (uintptr_t vma, size_t size, uint_fast8_t rwxug_flags) {
    vma = round_up_ptr(vma, PAGE_SIZE);
    size = round_up_size(size, PAGE_SIZE);
    for (size_t addr_idx = 0; addr_idx < size; addr_idx += PAGE_SIZE) {
        uintptr_t cur_vma = vma + addr_idx;
        uintptr_t newly_allocated = (uintptr_t *)(memory_alloc_page());
        struct pte* dest_pte = (struct pte*)walk_pt(active_space_root(), cur_vma, CREATE_PTE);
        dest_pte->flags |= rwxug_flags | PTE_A | PTE_D | PTE_V;
        dest_pte->ppn = pageptr_to_pagenum(newly_allocated);
        kprintf("assigning physical addr %x to virtual mem addr %x \n", newly_allocated, cur_vma);
        sfence_vma();
    }
    return vma;
}

void memory_set_page_flags(const void *vp, uint8_t rwxug_flags) {
    struct pte* dest_pte = (struct pte*)walk_pt(active_space_root(), vp, CREATE_PTE);
    dest_pte->flags = 0;
    dest_pte->flags |= rwxug_flags | PTE_A | PTE_D | PTE_V;
    sfence_vma();
}

void memory_set_range_flags (const void * vp, size_t size, uint_fast8_t rwxug_flags) {
    size = round_up_size(size, PAGE_SIZE);
    for (size_t addr_idx = 0; addr_idx < size; addr_idx += PAGE_SIZE) {
        uintptr_t cur_vma = vp + addr_idx;
        
        struct pte* dest_pte = (struct pte*)walk_pt(active_space_root(), cur_vma, 0);
        dest_pte->flags = 0;
        dest_pte->flags |= rwxug_flags | PTE_A | PTE_D | PTE_V;
        sfence_vma();
    }
}

void memory_unmap_and_free_user(void) {
    union linked_page* tail_free_list = free_list;
    if (tail_free_list != NULL) {
        while (tail_free_list->next != NULL) {
            tail_free_list = tail_free_list->next;
        }
    }

    // Loops through all three directories for every PTE with user tag U
    struct pte* cur_active_pt2 = active_space_root();
    for (size_t pt2_idx = 0; pt2_idx < PTE_CNT; pt2_idx++) {
        if ((cur_active_pt2[pt2_idx].flags & PTE_V) != 0 && (cur_active_pt2[pt2_idx].flags & PTE_X) == 0 && (cur_active_pt2[pt2_idx].flags & PTE_R) == 0 && (cur_active_pt2[pt2_idx].flags & PTE_W) == 0) {
            struct pte* subdirectory_pt1 = pagenum_to_pageptr(cur_active_pt2[pt2_idx].ppn);
            for (size_t pt1_idx = 0; pt1_idx < PTE_CNT; pt1_idx++) {
                if ((subdirectory_pt1[pt1_idx].flags & PTE_V) != 0 && (subdirectory_pt1[pt1_idx].flags & PTE_X) == 0 && (subdirectory_pt1[pt1_idx].flags & PTE_R) == 0 && (subdirectory_pt1[pt1_idx].flags & PTE_W) == 0) {
                    struct pte* leafdirectory_pt0 = pagenum_to_pageptr(subdirectory_pt1[pt1_idx].ppn);
                    for (size_t pt0_idx = 0; pt0_idx < PTE_CNT; pt0_idx++) {
                        if ((leafdirectory_pt0[pt0_idx].flags & PTE_V) != 0 && leafdirectory_pt0[pt0_idx].ppn != NULL && (leafdirectory_pt0[pt0_idx].flags & PTE_U) != 0) {
                            if (leafdirectory_pt0[pt0_idx].flags & PTE_U != 0) {
                                if (tail_free_list == NULL) {
                                    tail_free_list = pagenum_to_pageptr(leafdirectory_pt0[pt0_idx].ppn);
                                    free_list = tail_free_list;
                                } else {
                                    tail_free_list->next = pagenum_to_pageptr(leafdirectory_pt0[pt0_idx].ppn);
                                    tail_free_list = tail_free_list->next;
                                }
                                leafdirectory_pt0[pt0_idx].ppn &= 0;
                                leafdirectory_pt0[pt0_idx].flags &= 0;
                                sfence_vma();
                            }
                        }
                    }
                    if ((subdirectory_pt1[pt1_idx].flags & PTE_U) != 0) {
                        memory_free_page(leafdirectory_pt0);
                        sfence_vma();
                    }
                }
            }
            if ((cur_active_pt2[pt2_idx].flags & PTE_U) != 0) {
                memory_free_page(subdirectory_pt1);
                sfence_vma();
            }
        }
    }
}

void memory_handle_page_fault(const void * vptr) {
    if (vptr >= USER_START_VMA && vptr <= USER_END_VMA) {
        struct pte* dest_pte = (struct pte*)walk_pt(active_space_root(), vptr, CREATE_PTE);
        uintptr_t newly_allocated = (uintptr_t *)(memory_alloc_page());
        if (dest_pte->ppn != NULL) {
            memory_free_page(dest_pte->ppn);
        }
        dest_pte->ppn = newly_allocated;
        dest_pte->flags |= PTE_V;
        sfence_vma();
    } else {
        panic("VPTR Cannot be Allocated\n");
    }
}

int memory_validate_vptr_len (const void * vp, size_t len, uint_fast8_t rwxug_flags) {
    return 0;
}

int memory_validate_vstr (const char * vs, uint_fast8_t ug_flags) {
    return 0;
}

// INTERNAL FUNCTION DEFINITIONS
//

static inline int wellformed_vma(uintptr_t vma) {
    // Address bits 63:38 must be all 0 or all 1
    uintptr_t const bits = (intptr_t)vma >> 38;
    return (!bits || !(bits+1));
}

static inline int wellformed_vptr(const void * vp) {
    return wellformed_vma((uintptr_t)vp);
}

static inline int aligned_addr(uintptr_t vma, size_t blksz) {
    return ((vma % blksz) == 0);
}

static inline int aligned_ptr(const void * p, size_t blksz) {
    return (aligned_addr((uintptr_t)p, blksz));
}

static inline int aligned_size(size_t size, size_t blksz) {
    return ((size % blksz) == 0);
}

static inline void * pagenum_to_pageptr(uintptr_t n) {
    return (void*)(n << PAGE_ORDER);
}

static inline uintptr_t pageptr_to_pagenum(const void * p) {
    return (uintptr_t)p >> PAGE_ORDER;
}

static inline void * round_up_ptr(void * p, size_t blksz) {
    return (void*)((uintptr_t)(p + blksz-1) / blksz * blksz);
}

static inline uintptr_t round_up_addr(uintptr_t addr, size_t blksz) {
    return ((addr + blksz-1) / blksz * blksz);
}

static inline size_t round_up_size(size_t n, size_t blksz) {
    return (n + blksz-1) / blksz * blksz;
}

static inline void * round_down_ptr(void * p, size_t blksz) {
    return (void*)((uintptr_t)p / blksz * blksz);
}

static inline size_t round_down_size(size_t n, size_t blksz) {
    return n / blksz * blksz;
}

static inline uintptr_t round_down_addr(uintptr_t addr, size_t blksz) {
    return (addr / blksz * blksz);
}

static inline struct pte leaf_pte (
    const void * pptr, uint_fast8_t rwxug_flags)
{
    return (struct pte) {
        .flags = rwxug_flags | PTE_A | PTE_D | PTE_V,
        .ppn = pageptr_to_pagenum(pptr)
    };
}

static inline struct pte ptab_pte (
    const struct pte * ptab, uint_fast8_t g_flag)
{
    return (struct pte) {
        .flags = g_flag | PTE_V,
        .ppn = pageptr_to_pagenum(ptab)
    };
}

static inline struct pte null_pte(void) {
    return (struct pte) { };
}

static inline void sfence_vma(void) {
    asm inline ("sfence.vma" ::: "memory");
}