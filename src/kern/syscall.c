// syscall.c - system calls
//

#include <stddef.h>
#include "trap.h"
#include "scnum.h"
#include "process.h"
#include "console.h"
#include "device.h"
#include "error.h"
#include "fs.h"
#include "io.h"
#include "memory.h"
#include "timer.h"

// Trap frame register indices
#define TFR_A0 10
#define TFR_A1 11
#define TFR_A2 12
#define TFR_A7 17

// Internal function definitions

// Exits the currently running process.
static int sysexit(void) {
    process_exit();
    return 0;
}

// Prints msg to the console.
static int sysmsgout(const char *msg) {
    // validate string
    int result;

    result = memory_validate_vstr(msg, PTE_U);

    if (result != 0)
        return result;

    kprintf("Thread <%s:%d> says: %s\n", thread_name(running_thread()), running_thread(), msg);
    return 0;
}

// Opens a device at the specified file descriptor and returns error code on failure.
static int sysdevopen(int fd, const char *name, int instno) {
    struct process * curproc = current_process();

    // boundary checks
    if (curproc == NULL)
        return -ENOENT;

    if (fd >= MAX_OPEN_FILE_CT)
        return -ENOENT;

    // fd < 0 require the next available file descriptor
    if (fd < 0) {
        for (fd = 0; fd < MAX_OPEN_FILE_CT; fd++) {
            if (curproc->iotab[fd] != NULL)
                break;
        }

        if (fd == MAX_OPEN_FILE_CT)
            return -ENOENT;
    }

    struct io_intf * device_io = curproc->iotab[fd];

    // if (device_io == NULL)
    //     return -EIO;

    int result = device_open(&device_io, name, instno);
    curproc->iotab[fd] = device_io;
    if (result < 0)
        return -ENODEV;

    return fd;
}

// Opens a file at the specified file descriptor and returns error code on failure.
static int sysfsopen(int fd, const char *name) {
    struct process * curproc = current_process();

    // boundary checks    
    if (curproc == NULL)
        return -ENOENT;

    if (fd >= MAX_OPEN_FILE_CT)
        return -ENOENT;

    // fd < 0 require the next available file descriptor
    if (fd < 0) {
        for (fd = 0; fd < MAX_OPEN_FILE_CT; fd++) {
            if (curproc->iotab[fd] != NULL)
                break;
        }

        if (fd == MAX_OPEN_FILE_CT)
            return -ENOENT;
    }

    struct io_intf * file_io = curproc->iotab[fd];

    // if (file_io == NULL)
    //     return -EIO;

    int result = fs_open(name, &file_io);
    curproc->iotab[fd] = file_io;
    if (result < 0)
        return -ENOENT;

    return fd;
}

// Closes the device at the specified file descriptor.
static int sysclose(int fd) {
    struct process * curproc = current_process();

    // boundary checks
    if (curproc == NULL)
        return -ENOENT;

    if (fd <0 || fd >= MAX_OPEN_FILE_CT)
        return -ENOENT;

    struct io_intf * io = curproc->iotab[fd];

    if (io == NULL)
        return -EIO;

    io->ops->close(io);

    curproc->iotab[fd] = NULL;

    return 0;
}

// Reads from the opened file descriptor and writes bufsz bytes into buf.
static long sysread(int fd, void *buf, size_t bufsz) {
    // validate buffer
    int validate_result;

    validate_result = memory_validate_vptr_len(buf, bufsz, PTE_U | PTE_W);

    if (validate_result != 0)
        return validate_result;

    struct process * curproc = current_process();

    // boundary checks
    if (curproc == NULL)
        return -ENOENT;

    if (fd <0 || fd >= MAX_OPEN_FILE_CT)
        return -ENOENT;

    struct io_intf * io = curproc->iotab[fd];

    if (io == NULL)
        return -EIO;

    return ioread(io, buf, bufsz);
}

// Reads bufsz bytes from buf and writes it to the opened file descriptor.
static long syswrite(int fd, const void *buf, size_t len) {        
    struct process * curproc = current_process();

    // boundary checks
    if (curproc == NULL)
        return -ENOENT;

    if (fd < 0 || fd >= MAX_OPEN_FILE_CT)
        return -ENOENT;

    struct io_intf * io = curproc->iotab[fd];

    if (io == NULL)
        return -EIO;

    return iowrite(io, buf, len);
}

// Performs desired ioctl based on cmd
static int sysioctl(int fd, int cmd, void *arg) {
    struct process * curproc = current_process();

    // boundary checks
    if (curproc == NULL)
        return -ENOENT;

    if (fd >= MAX_OPEN_FILE_CT)
        return -ENOENT;

    // fd < 0 require the next available file descriptor
    if (fd < 0) {
        for (fd = 0; fd < MAX_OPEN_FILE_CT; fd++) {
            if (curproc->iotab[fd] != NULL)
                break;
        }

        if (fd == MAX_OPEN_FILE_CT)
            return -ENOENT;
    }

    struct io_intf * io = curproc->iotab[fd];

    if (io == NULL)
        return -EIO;

    return ioctl(io, cmd, arg);
}

// Halts currently running user program and starts new program based on opened file at file descriptor.
static int sysexec(int fd) {
    struct process * curproc = current_process();

    // boundary checks
    if (curproc == NULL)
        return -ENOENT;

    if (fd < 0 || fd >= MAX_OPEN_FILE_CT)
        return -ENOENT;

    // fd < 0 require the next available file descriptor
    // if (fd < 0) {
    //     for (fd = 0; fd < MAX_OPEN_FILE_CT; fd++) {
    //         if (curproc->iotab[fd] != NULL)
    //             break;
    //     }

    //     if (fd == MAX_OPEN_FILE_CT)
    //         return -ENOENT;
    // }

    struct io_intf * exeio = curproc->iotab[fd];

    if (exeio == NULL)
        return -EIO;

    return process_exec(exeio);
}

// Wait for certain child to exit before returning. 
// If tid is the main thread, wait for any child of current
// thread to exit.
static int syswait(int tid) {

    trace("%s(%d)", __func__, tid);

    if (tid == 0)
        return thread_join_any();
    else
        return thread_join(tid);
}

// Sleep for us number of microseconds
static int sysusleep(unsigned long us) {
    struct alarm * al = kmalloc(sizeof(struct alarm));
    alarm_init(al, "alarm_us");
    alarm_sleep_us(al, us);
    return 0;
}


// Called from the usermode exception handler to handle all syscalls. Jumps to a system call based on
// the specified system call number
void syscall_handler(struct trap_frame * tfr) {
    // kprintf("enter syscall handler\n");
    tfr->sepc += 4;
    int scnum = tfr->x[TFR_A7];

    switch (scnum) {
        case SYSCALL_EXIT:
            tfr->x[TFR_A0] = sysexit();
            break;
        case SYSCALL_MSGOUT:
            tfr->x[TFR_A0] = sysmsgout((const char *) tfr->x[TFR_A0]);
            break;
        case SYSCALL_DEVOPEN:
            tfr->x[TFR_A0] = sysdevopen(tfr->x[TFR_A0], (const char *) tfr->x[TFR_A1], tfr->x[TFR_A2]);
            break;
        case SYSCALL_FSOPEN:
            tfr->x[TFR_A0] = sysfsopen(tfr->x[TFR_A0], (const char *) tfr->x[TFR_A1]);
            break;
        case SYSCALL_CLOSE:
            tfr->x[TFR_A0] = sysclose(tfr->x[TFR_A0]);
            break;
        case SYSCALL_READ:
            tfr->x[TFR_A0] = sysread(tfr->x[TFR_A0], (void *) tfr->x[TFR_A1], tfr->x[TFR_A2]);
            break;
        case SYSCALL_WRITE:
            tfr->x[TFR_A0] = syswrite(tfr->x[TFR_A0], (const void *) tfr->x[TFR_A1], tfr->x[TFR_A2]);
            break;
        case SYSCALL_IOCTL:
            tfr->x[TFR_A0] = sysioctl(tfr->x[TFR_A0], tfr->x[TFR_A1], (void *) tfr->x[TFR_A2]);
            break;
        case SYSCALL_EXEC:
            tfr->x[TFR_A0] = sysexec(tfr->x[TFR_A0]);
            break;
        case SYSCALL_WAIT:
            tfr->x[TFR_A0] = syswait(tfr->x[TFR_A0]);
        case SYSCALL_USLEEP:
            tfr->x[TFR_A0] = sysusleep(tfr->x[TFR_A0]);
        default:
            tfr->x[TFR_A0] = -1;
            break;
    }
}