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

// Get input from the user
static int sysmsgin(char * msg, size_t n) {
    console_getsn(msg, n);
    return 0;
}

// Prints msg to the console.
static int sysmsgout(const char *msg) {
    console_puts(msg);
    return 0;
}

// Opens a device at the specified file descriptor and returns error code on failure.
static int sysdevopen(int fd, const char *name, int instno) {
    struct process * curproc = current_process();

    // boundary checks
    if (curproc == NULL)
        return -ENOENT;

    if (fd < 0 || fd >= MAX_OPEN_FILE_CT)
        return -ENOENT;

    struct io_intf * device_io = curproc->iotab[fd];

    if (device_io == NULL)
        return -EIO;

    int result = device_open(&device_io, name, instno);

    if (result < 0)
        return -ENODEV;

    return 0;
}

// Opens a file at the specified file descriptor and returns error code on failure.
static int sysfsopen(int fd, const char *name) {
    struct process * curproc = current_process();

    // boundary checks    
    if (curproc == NULL)
        return -ENOENT;

    if (fd < 0 || fd >= MAX_OPEN_FILE_CT)
        return -ENOENT;

    struct io_intf * file_io = curproc->iotab[fd];

    if (file_io == NULL)
        return -EIO;

    int result = fs_open(&file_io, name);

    if (result < 0)
        return -ENOENT;

    return 0;
}

// Closes the device at the specified file descriptor.
static int sysclose(int fd) {
    struct process * curproc = current_process();

    // boundary checks
    if (curproc == NULL)
        return -ENOENT;

    if (fd < 0 || fd >= MAX_OPEN_FILE_CT)
        return -ENOENT;

    struct io_intf * io = curproc->iotab[fd];

    if (io == NULL)
        return -EIO;

    io->ops->close(io);

    return 0;
}

// Reads from the opened file descriptor and writes bufsz bytes into buf.
static long sysread(int fd, void *buf, size_t bufsz) {
    struct process * curproc = current_process();

    // boundary checks
    if (curproc == NULL)
        return -ENOENT;

    if (fd < 0 || fd >= MAX_OPEN_FILE_CT)
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

    if (fd < 0 || fd >= MAX_OPEN_FILE_CT)
        return -ENOENT;

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

    struct io_intf * exeio = curproc->iotab[fd];

    if (exeio == NULL)
        return -EIO;

    return process_exec(exeio);
}

// Called from the usermode exception handler to handle all syscalls. Jumps to a system call based on
// the specified system call number
void syscall_handler(struct trap_frame * tfr) {
    int scnum = tfr->x[TFR_A7];

    switch (scnum) {
        case SYSCALL_EXIT:
            tfr->x[TFR_A0] = sysexit();
            break;
        case SYSCALL_MSGOUT:
            tfr->x[TFR_A0] = sysmsgout((const char *) tfr->x[TFR_A0]);
            break;
        case SYSCALL_MSGIN:
            tfr->x[TFR_A0] = sysmsgin((char *) tfr->x[TFR_A0], tfr->x[TFR_A1]);
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
        default:
            tfr->x[TFR_A0] = -1;
            break;
    }
}