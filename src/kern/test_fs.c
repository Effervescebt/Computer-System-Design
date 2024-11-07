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


extern char _companion_f_start[];
extern char _companion_f_end[];

void main() {
    struct io_lit lit;
    lit.buf = NULL;
    lit.pos = 0;
    lit.size = 0;

    heap_init(_kimg_end, (void*)RAM_START+RAM_SIZE);
    size_t start_pos = 0;
    char * read_buf = (char *)kmalloc(351);
    char * read_buf2 = (char *)kmalloc(120);
    char * read_buf_honor1 = (char *)kmalloc(339);
    char * read_buf_horus1 = (char *)kmalloc(27414);
    char * read_buf_horus2 = (char *)kmalloc(27722);
    char * read_buf_horus2_modified = (char *)kmalloc(27722);
    char * read_buf_horus_split_read = (char *)kmalloc(10000);
    //console_printf("%x %x %x %x buf addr\n", read_buf, read_buf_honor1, read_buf_horus1, read_buf_horus2);
    //console_printf("%x \n", _companion_f_start);
    //for (int i = 0; i < 20480; i++) {
    //    console_printf("%d ", *(_companion_f_start + i));
    //}
    struct io_intf* file_io = iolit_init(&lit, _companion_f_start, _companion_f_end - _companion_f_start);
    struct io_intf* hellow_io;
    struct io_intf* honor_io;
    struct io_intf* horus_io;
    struct io_intf* horus_io_ch2;
    
    fs_mount(file_io);
    struct boot_block_t * boot_blk = kmalloc(sizeof(struct boot_block_t));
    ioseek(file_io, start_pos);
    ioread(file_io, boot_blk, FS_BLKSZ);
    ioseek(file_io, FS_BLKSZ);
    //console_printf("%d %d %d", boot_blk->num_dentry, boot_blk->num_inodes, boot_blk->num_data);
    if (boot_blk->num_dentry != 7) panic("Wrong dentry\n");
    if (boot_blk->num_inodes != 7) panic("Wrong inodes\n");
    if (boot_blk->num_data != 130) panic("Wrong numdata\n");
    //console_printf("%d current1 lit size\n", lit.size);
    fs_open("HelloWorld.txt", &hellow_io);
    fs_open("HonorAndDeath.txt", &honor_io);
    fs_open("HorusHeresyChOne.txt", &horus_io);
    fs_open("HorusHeresyChTwo.txt", &horus_io_ch2);

    if (honor_io == hellow_io) {
        //console_printf("wrong, same io\n");
    } else {
        //console_printf("correct, different io\n");
    }
    console_printf("\n");
    console_printf("SHORT FILE TESTS\n");
    console_printf("\n");
    console_printf("\n");
    //console_printf("reading first file\n");
    fs_read(hellow_io, read_buf, 351);
    console_printf("File EMPEROR PROTECTS\n");
    console_printf("\n");
    for (int i = 0; i < 351; i++) {
        console_putchar(*(read_buf + i));
    }
    //console_puts(read_buf);
    console_printf("\n");
    console_printf("\n");
    fs_read(honor_io, read_buf_honor1, 339);
    console_printf("File PRAISE EMPEROR\n");
    console_printf("\n");
    for (int i = 0; i < 339; i++) {
        console_putchar(*(read_buf_honor1 + i));
    }
    console_printf("\n");
    console_printf("\n");
    
    //console_printf("file test setting position\n");
    ioctl(hellow_io, IOCTL_SETPOS, &start_pos);
    iowrite(hellow_io, read_buf_honor1, 160);
    console_printf("SHORT FILE WRITE TEST\n");
    ioctl(hellow_io, IOCTL_SETPOS, &start_pos);
    ioread_full(hellow_io, read_buf2, 351);
    for (int i = 0; i < 351; i++) {
        console_putchar(*(read_buf2 + i));
    }
    console_printf("\n");
    console_printf("\n");
    //console_printf("address of buff %x %x \n", read_buf, read_buf_honor1);
    console_printf("LONG PARAGRAPH CHECK I\n");
    console_printf("FILE TEST READING HORUS HERESY I\n");
    console_printf("\n");
    ioread_full(horus_io, read_buf_horus1, 27414);
    for (int i = 0; i < 27414; i++) {
        console_putchar(*(read_buf_horus1 + i));
    }
    console_printf("\n");
    console_printf("\n");
    console_printf("LONG PARAGRAPH CHECK II (CROSS BLOCK)\n");
    ioseek(horus_io, 0);
    ioread_full(horus_io, read_buf_horus_split_read, 1500);
    ioread_full(horus_io, read_buf_horus_split_read + 1500, 8500);
    console_printf("FILE TEST READING HORUS HERESY I IN CROSS BLOCK CASE\n");
    console_printf("\n");
    //console_putchar(*(_companion_f_start + 24576));
    for (int i = 0; i < 10000; i++) {
        console_putchar(*(read_buf_horus_split_read + i));
    }
    console_printf("\n");
    console_printf("\n");
    console_printf("LONG PARAGRAPH CHECK III\n");
    console_printf("FILE TEST READING HORUS HERESY II\n");
    console_printf("\n");
    ioread_full(horus_io_ch2, read_buf_horus2, 27722);
    for (int i = 0; i < 27722; i++) {
        console_putchar(*(read_buf_horus2 + i));
    }
    console_printf("\n");
    console_printf("\n");
    console_printf("LONG PARAGRAPH CHECK IV (CROSS BLOCK)\n");
    console_printf("FILE TEST WRITING HORUS HESERY I TO HORUS HERESY II\n");
    console_printf("\n");
    ioctl(horus_io_ch2, IOCTL_SETPOS, &start_pos);
    iowrite(horus_io_ch2, read_buf_horus_split_read, 1500);
    iowrite(horus_io_ch2, read_buf_horus_split_read + 1500, 8500);
    ioctl(horus_io_ch2, IOCTL_SETPOS, &start_pos);
    ioread_full(horus_io_ch2, read_buf_horus2_modified, 27722);
    for (int i = 0; i < 27722; i++) {
        console_putchar(*(read_buf_horus2_modified + i));
    }
    console_printf("\n");
    console_printf("\n");
}