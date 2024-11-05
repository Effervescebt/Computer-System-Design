//           main.c - Main function: runs shell to load executable
//          

#include "console.h"
#include "vioblk.c"
#include "thread.h"

//           end of kernel image (defined in kernel.ld)
extern char _kimg_end[];

#define RAM_SIZE (8*1024*1024)
#define RAM_START 0x80000000UL
#define KERN_START RAM_START
#define USER_START 0x80100000UL

#define UART0_IOBASE 0x10000000
#define UART1_IOBASE 0x10000100
#define UART0_IRQNO 10

#define VIRT0_IOBASE 0x10001000
#define VIRT1_IOBASE 0x10002000
#define VIRT0_IRQNO 1


void main(void) {
    struct io_intf * blkio;
    struct io_intf * termio;
    void * mmio_base;
    int result;
    int i;

    heap_init(_kimg_end, (void*)USER_START);
    thread_init();
    
    //           Attach virtio devices

    for (i = 0; i < 8; i++) {
        mmio_base = (void*)VIRT0_IOBASE;
        mmio_base += (VIRT1_IOBASE-VIRT0_IOBASE)*i;
        virtio_attach(mmio_base, VIRT0_IRQNO+i);
    }

    intr_enable();

    // vioblk open test
    result = device_open(&blkio, "blk", 0);
    if (result != 0)
        panic("device_open failed");

    void (*exe_entry)(struct io_intf*);
    char cmdbuf[9];
    int tid = thread_spawn(cmdbuf, (void*)exe_entry, termio);

    void* buf;
    unsigned long bufsz = 500;
    uint64_t posptr; 
    uint64_t lenptr;
    uint32_t blkszptr;

    vioblk_ioctl(blkio, IOCTL_GETBLKSZ, &blkszptr);  //vioblk ioctl getblksz
    if (blkszptr != 512)   panic("vioblk_ioctl getblksz failed");
    console_printf("vioblk_ioctl getblksz pass\n");

    vioblk_ioctl(blkio, IOCTL_GETLEN, &lenptr); // vioblk ioctl getlen
    console_printf("getlen: %d\n", lenptr);

    // vioblk read test
    vioblk_ioctl(blkio, IOCTL_GETPOS, &posptr);  // vioblk ioctl getpos
    if (posptr != 0)   panic("vioblk_ioctl getpos failed");
    console_printf("vioblk_ioctl getpos pass\n");

    vioblk_read(blkio, buf, bufsz);
    vioblk_ioctl(blkio, IOCTL_GETPOS, &posptr);
    if (posptr != 500)   panic("vioblk_read failed");
    console_printf("vioblk read pass\n");


    // vioblk write test
    posptr = 200;
    vioblk_ioctl(blkio, IOCTL_SETPOS, &posptr);  // vioblk ioctrl setpos
    if (vioblk_ioctl(blkio, IOCTL_GETPOS, &posptr) != 200)
        panic("vioblk_ioctl setpos failed");
    console_printf("vioblk_ioctl setpos pass\n");

    vioblk_write(blkio, buf, bufsz);
    vioblk_ioctl(blkio, IOCTL_GETPOS, &posptr);
    if (posptr != 200 + 500)   panic("vioblk_write failed");
    console_printf("vioblk write pass\n");

    vioblk_close(blkio);
}