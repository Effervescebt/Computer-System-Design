//           fs.h - File system interface
//      
#include "io.h"
#include "error.h"
#include "string.h"
#include <stddef.h>
#include <stdint.h>
#include "lock.h"

#ifndef _FS_H_
#define _FS_H_

#define BOOT_MINLEN 4
#define POINTER_SZ 8
#define SHIFT_MAG 256
#define DIR_ENTRY_SZ 64
#define DIR_ENTRY_CT 63
#define DIR_ENTRY_RESERVED 28
#define MAX_OPEN_FILE_CT 32
#define IN_USE 1
#define UNUSED 0
#define DATA_BLOCK_NUM 1023
#define FS_BLKSZ      4096
#define FS_NAMELEN    32
#define NEXT_DATA_BLK_IDX 1


extern char _kimg_end[]; // end of kernel image (defined in kernel.ld)

#ifndef RAM_SIZE
#ifdef RAM_SIZE_MB
#define RAM_SIZE (RAM_SIZE_MB*1024*1024)
#else
#define RAM_SIZE (8*1024*1024)
#endif
#endif

#ifndef RAM_START
#define RAM_START 0x80000000UL
#endif


typedef struct dentry_t{
    char file_name[FS_NAMELEN];
    uint32_t inode;
    uint8_t reserved[28];
}__attribute((packed)) dentry_t; 

typedef struct boot_block_t{
    uint32_t num_dentry;
    uint32_t num_inodes;
    uint32_t num_data;
    uint8_t reserved[52];
    dentry_t dir_entries[63];
}__attribute((packed)) boot_block_t;

typedef struct inode_t{
    uint32_t byte_len;
    uint32_t data_block_num[1023];
}__attribute((packed)) inode_t;

typedef struct data_block_t{
    uint8_t data[FS_BLKSZ];
}__attribute((packed)) data_block_t;


typedef struct file_t{
    struct io_intf* io_intf;
    uint64_t file_position;
    uint64_t file_size;
    uint64_t inode;
    uint64_t usage_flag;
}__attribute((packed)) file_t;

typedef struct file_array{
    file_t current_opened_files[MAX_OPEN_FILE_CT];
}__attribute((packed)) file_array;  

extern char fs_initialized;

extern void fs_init(void);
extern int fs_mount(struct io_intf * blkio);
extern int fs_open(const char * name, struct io_intf ** ioptr);
extern void fs_close(struct io_intf* io);
extern int fs_getlen(file_t* fd, void* arg);
extern int fs_getpos(file_t* fd, void* arg);
extern int fs_setpos(file_t* fd, void* arg);
extern int fs_getblksz(file_t* fd, void* arg);
extern int fs_ioctl(struct io_intf* io, int cmd, void* arg);
extern long fs_write(struct io_intf* io, const void* buf, unsigned long n);
extern long fs_read(struct io_intf* io, void* buf, unsigned long n);


//           _FS_H_
#endif