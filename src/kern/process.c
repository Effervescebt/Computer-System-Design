// process.c - user process
//

#ifdef PROCESS_TRACE
#define TRACE
#endif

#ifdef PROCESS_DEBUG
#define DEBUG
#endif


// COMPILE-TIME PARAMETERS
//

// NPROC is the maximum number of processes

#ifndef NPROC
#define NPROC 16
#endif

// INTERNAL FUNCTION DECLARATIONS
//

// INTERNAL GLOBAL VARIABLES
//

#define MAIN_PID 0

// The main user process struct

static struct process main_proc;

// A table of pointers to all user processes in the system

struct process * proctab[NPROC] = {
    [MAIN_PID] = &main_proc
};

// EXPORTED GLOBAL VARIABLES
//

char procmgr_initialized = 0;

// EXPORTED FUNCTION DEFINITIONS
//
#include "process.h"
#define ENOMEM 11

void procmgr_init(void){
    // for (int i = 0; i < NPROC; i++) {
    //     proctab[i] = NULL;
    // }
    // First clear the process struct
    memset(&main_proc, 0, sizeof(main_proc));
    // Initialize PID to 0
    main_proc.id = MAIN_PID;
    // Set the associated thread id
    main_proc.tid = running_thread();
    main_proc.mtag = memory_space_create(0);
    // Clean iotab
    for (int i = 0; i < PROCESS_IOMAX; i++){
        main_proc.iotab[i] = NULL;
    }
    // update to 1 (this variable is used as flag here)
    procmgr_initialized = 1;
}

int process_exec(struct io_intf * exeio){
    // First check arg
    if (!exeio){
        return -EINVAL;
    }
    // Then unmap virtual memory mapping of other user process
    memory_unmap_and_free_user();
    // Initialize a new ptr for process 
    struct process* cur_proc = NULL;
    // Now we should first check and find avaible slot for user process
    for (int i = 0; i < NPROC; i++){
        // If we find the empty slot
        if (proctab[i] == NULL){
            // TBD 2
            cur_proc = kmalloc(sizeof(struct process));
            // Return if fail to allocate space
            if (!cur_proc){
                // Memory allocate failure
                return -EACCESS;
            }
            // If successfully allocate space
            // 1. Clear it
            memset(cur_proc, 0, sizeof(struct process));
            // 2. allocate process id 
            cur_proc->id = i;
            // 3. allocate thread id
            // struct thread *current_thread = CURTHR;
            // int tid = current_thread->id;
            // #define CURTHR ((struct thread*)__builtin_thread_pointer())
            // TBD 3
            cur_proc->tid = 0;
            // Create root page table
            uintptr_t new_page_table = memory_space_create(cur_proc->id);
            if (!new_page_table){
                kfree(cur_proc);
                proctab[i] = NULL;
                return -ENOMEM;
            }
            cur_proc->mtag = new_page_table;
            //struct pte *root = mtag_to_root(new_page_table);
            proctab[i] = cur_proc;
            console_printf("Process initialized. proc_num: %d\n", i);
            break;
        }
    }
    // Check if reach max proc cnt
    if (!cur_proc) {
        return -EMFILE; 
    }
    // Initialize entry point
    void (*entry_point)(void) = NULL;
    // Run elf_load to update entry_point
    int elf_result = elf_load(exeio, &entry_point);
    // Check elf_result
    if (elf_result < 0) {
        memory_space_reclaim(); 
        kfree(cur_proc);                      
        proctab[cur_proc->id] = NULL;         
        return elf_result;                    
    }
    console_printf("Elf successfully loaded. Entry point: %p\n", entry_point);


    return 0;
}

void process_exit(){
    // First get current process
    struct process* cur_prog = current_process();
    // checklist of release: process memory space, I/O interface, associated kernel thread
    if (!cur_prog){
        panic("No current process exist");
    }
    // Release memory space
    // TBD 4
    memory_space_switch(cur_prog->mtag);
    // Release I/O interfaces
    for (int i = 0; i < NPROC; i++){
        if (cur_prog->iotab[i]){
            ioclose(cur_prog->iotab[i]);
            cur_prog->iotab[i] == NULL;
        }
    }
    // mark it empty in the process table
    proctab[cur_prog->id] = NULL;
    kfree(cur_prog);
    // Exit the thread
    thread_exit();
}