// reads the boot block and lists all the files on the filesystem
//

#include "syscall.h"
#include "string.h"
#include "io.h"

// other constants
#define BLKSZ 4096
#define BOOTBLKSZ BLKSZ*16

struct boot_block_t {
    int num_dentry;
    int num_inodes;
    int num_data;
    char reserved[52];
    struct dentry_t {
        char file_name[32];
        int inode;
        char reserved[28];
    } dir_entries[63];
};

void main(void) {

    int result;

    // Open ser1 device as fd=0
    result = _devopen(0, "blk", 0);
    if (result < 0) {
        _msgout("_devopen failed");
        _exit();
    }

    // read the boot block
    struct boot_block_t * boot_blk = kmalloc(BOOTBLKSZ);
    int pos = 0;
    _ioctl(0, IOCTL_SETPOS, &pos);
    _read(0, boot_blk, BOOTBLKSZ);
    int num_dentry = boot_blk->num_dentry;

    // print the files
    for (int i = 0; i < num_dentry; i++)
        _msgout(boot_blk->dir_entries[i].file_name);
}
