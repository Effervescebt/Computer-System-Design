//           main.c - Main function: runs shell to load executable
//          

#include "console.h"
#include "thread.h"
#include "device.h"
#include "uart.h"
#include "timer.h"
#include "intr.h"
#include "heap.h"
#include "virtio.h"
#include "halt.h"
#include "elf.h"
#include "fs.h"
#include "string.h"

//           end of kernel image (defined in kernel.ld)
extern char _kimg_end[];

#define KERN_START RAM_START
#define USER_START 0x80100000UL

#define VIRT0_IOBASE 0x10001000
#define VIRT1_IOBASE 0x10002000
#define VIRT0_IRQNO 1

#define VIOBLK_SECTOR_SIZE      512

void main(void) {
    struct io_intf * blkio;
    void * mmio_base;
    int result;
    int i;

    intr_init();
    thread_init();

    heap_init(_kimg_end, (void*)USER_START);

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

    unsigned long bufsz = 4096;
    uint64_t posptr = 0; 
    uint64_t lenptr = 0;
    uint32_t blkszptr = 0;
    char* buf = (char*) kmalloc(bufsz);

    // vioblk ioctl getblksz
    // blksz always 512
    ioctl(blkio, IOCTL_GETBLKSZ, &blkszptr);  
    console_printf("blksz: %d\n", blkszptr);
    if (blkszptr != VIOBLK_SECTOR_SIZE)   panic("vioblk_ioctl getblksz failed");
    console_printf("vioblk_ioctl getblksz pass\n");

    // vioblk ioctl getlen
    ioctl(blkio, IOCTL_GETLEN, &lenptr); 
    console_printf("getlen: %d\n", lenptr);
    console_printf("vioblk_ioctl getlen pass\n");

    // vioblk ioctl getpos
    // posptr should be 0 as initialized
    ioctl(blkio, IOCTL_GETPOS, &posptr);  
    if (posptr != 0)   panic("vioblk_ioctl getpos failed");
    console_printf("vioblk_ioctl getpos pass\n");

    // vioblk setpos
    // set pos to 100
    posptr = 100;
    ioctl(blkio, IOCTL_SETPOS, &posptr);
    if (ioctl(blkio, IOCTL_GETPOS, &posptr) != 100)   panic("vioblk_ioctl setpos failed");
    console_printf("vioblk_ioctl setpos pass\n");

    // vioblk read
    // reset pos and read 4096 chars
    // same as kfs.raw in hex editor
    posptr = 0;
    ioctl(blkio, IOCTL_SETPOS, &posptr);
    result = ioread(blkio, buf, bufsz);
    console_printf("vioblk read pass\n");

    // detailed test to print what we read in char and hex form
    // if (result != bufsz)    panic("vioblk read failed");
    // for (int i = 0; i < bufsz; i++)
    //     console_putchar(*(buf + i));    // char form
    // console_printf("\n");
    // for (int i = 0; i < bufsz; i++)
    //     console_printf("%x ", *(buf + i));      // hex form
    // console_printf("\n");

    // vioblk write
    // now pos at 4096, write at pos
    // kfs.raw row 0x1000 written

    // additional test to write anywhere you want by setting posptr
    // posptr = 0;
    // ioctl(blkio, IOCTL_SETPOS, &posptr);

    char* writebuf = (char*) kmalloc(12);
    writebuf = "Hello World!";
    result = iowrite(blkio, writebuf, 12);
    if (result != 12)    panic("vioblk write failed");
    console_printf("vioblk write pass\n");

    ioclose(blkio);
    kfree(buf);
    kfree(writebuf);
}