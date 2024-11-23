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

/**
 * @brief: This function initialize the main user process
 * 
 * We clear the structure and fill in all param.
 * And set the progcmgr_initialized flag
 * 
 */
void procmgr_init(void){
    memset(&main_proc, 0, sizeof(main_proc));
    // Initialize PID to 0
    main_proc.id = MAIN_PID;
    // Set the associated thread id and mtag
    main_proc.tid = running_thread();
    main_proc.mtag = main_mtag;
    // Clean iotab
    for (int i = 0; i < PROCESS_IOMAX; i++){
        main_proc.iotab[i] = NULL;
    }
    // TBD 1
    // update to 1 (this variable is used as flag here)
    procmgr_initialized = 1;
    console_printf("Main process initialized successfully.\n");
}

/**
 * @brief: This function takes I/O interface and the execute the program it refer to.
 * @param: exeio: the passed I/O interface
 * 
 * This function performs the following steps:
 * 1. Unmap virtual memory mapping to other user process
 * 2. Create a new memory space(not needed for this cp)
 * 3. Load the executable from the passed-in I/O interface
 * 4. Start the associated thread in U mode
 * 
 * @return: error code or 0 for success
 */
int process_exec(struct io_intf * exeio){
    // First check arg
    if (!exeio){
        return -EINVAL;
    }
    // Then unmap virtual memory mapping of other user process
    memory_unmap_and_free_user();
    
    // Initialize entry point
    void (*entry_point)(void) = NULL;
    // Run elf_load to update entry_point
    int elf_result = elf_load(exeio, &entry_point);
    // Check elf_result
    if (elf_result < 0) {       
        return elf_result;                    
    }
    console_printf("Elf successfully loaded. Entry point: %p\n", entry_point);

    // This is the staring pt of user stack
    uintptr_t usp = USER_STACK_VMA;
    // Now change to user mode
    thread_jump_to_user(usp, &entry_point);
    console_printf("Fail to U mode\n");
    return -EINVAL;
}

/**
 * @brief: This function clean up a finished process
 * 
 * Relase following: 1. Process memory space
 * 2. Open I/O interfaces
 * 3. Associated kernel thread
 * 
 * @note: For process memory space, unmap the user space before calling switch function (TBD)
 */
void process_exit(){
    // First get current process
    struct process* cur_prog = current_process();
    // checklist of release: process memory space, I/O interface, associated kernel thread
    if (!cur_prog){
        panic("No current process exist");
    }

    // Release memory space
    memory_space_reclaim();

    // Release I/O interfaces
    for (int i = 0; i < NPROC; i++){
        if (cur_prog->iotab[i]){
            ioclose(cur_prog->iotab[i]);
            cur_prog->iotab[i] = NULL;
        }
    }
    // mark it empty in the process table
    proctab[cur_prog->id] = NULL;
    //kfree(cur_prog);
    
    // Exit the thread
    thread_exit();
}