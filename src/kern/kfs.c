#include "fs.h"
#include "console.h"

static struct file_array opened_files;
static struct io_intf * system_io;
static struct boot_block_t super_block;

extern void * kmalloc(size_t size);

/*
 * @brief Takes an io intf* to the filesystem provider and sets up the filesystem for future fs open operations.
 *
 * @specific
 * -> fs_mount accepts an io interface, which will be the system io of current file system
 * -> mount simply initialize the boot_block, initialize free space in heap
 * -> reads boot_block by system io's read function, which reads through the file system memory, and set buffer to the boot_block declared in fs.h
 * 
 * @param
 * -> struct io_intf* io, system io interface
 * 
 * @return val
 * -> 0
 */
int fs_mount(struct io_intf* io) {
    // set system io, will be used in most functions
    system_io = io;
    // read the first FS_BLKSZ length data, which is the boot_block we need, into the global boot_block
    long read_result = ioread_full(io, &super_block, FS_BLKSZ);
    if (read_result < 0) {
        return - EFILESYS;
    }
    // struct io_lit * lit = system_io;
    return 0;
}

/*
 * @brief Takes the name of the file to be opened and modifies the given pointer to contain the io intf of the file.
 *
 * @specific
 * -> fs_open accepts a name and double pointer, the name used to search for the file to open, and the pointer will be implemented and stored as the file's io interface
 * -> the filesystem has a file array that holds up to 32 opened files (struct called file_t), and is a struct file_array called opened_files in this context
 * -> first search through the boot_block for file name (compare strings using strcmp). Once the file found, extract its inode for memory read used
 * -> set system_io's read function to position where it reads corresponding inode block, use system_io read to read the data into an inode_t struct
 * -> malloc free space to store a newly defined struct io_intf* io, this will be the io_intf of this file. Point the given double pointer to this io pointer.
 * -> find the first empty slot in the file array, modify its contents (a file_t struct) to hold current file
 * 
 * @param
 * -> const char* name, name of the file to open
 * -> struct io_intf** io, io of the file to open, empty when passed to this function
 * 
 * @return val
 * -> 0 or Error Code
 */
int fs_open(const char* name, struct io_intf** io) {
    uint32_t requested_inode = -1;
    uint64_t inode_position = 0;
    // find the inode of the file to open
    for (int i = 0; i < DIR_ENTRY_CT; i++) {
        if (strcmp(super_block.dir_entries[i].file_name, name) == 0) {
            requested_inode = super_block.dir_entries[i].inode;
            break;
        }
    }
    if (requested_inode == -1) {
        return -ENOENT;
    }
    
    // calculate the inode block location in file system memory
    size_t buffer_idx = FS_BLKSZ * requested_inode + FS_BLKSZ;
    // set read start position in the system_io
    ioctl(system_io, IOCTL_SETPOS, &buffer_idx);
    struct inode_t * file_struct = kmalloc(sizeof(struct inode_t));
    // this will read the needed inode block
    long read_result = system_io->ops->read(system_io, file_struct, FS_BLKSZ);
    if (read_result < 0) {
        return -EFILESYS;
    }

    // malloc a pointer to io_intf on heap, save the system_io's ops functions in this io_intf
    struct io_intf* new_io = kmalloc(sizeof(struct io_intf));
    // save the newly created io_intf by pointing given double pointer io to new_io
    static const struct io_ops new_file_ops = {
        .read = fs_read,
        .write = fs_write,
        .close = fs_close,
        .ctl = fs_ioctl
    };
    new_io->ops = &new_file_ops;
    new_io->refcnt = 1; // setting refcnt for new io
    *io = new_io;

    // find the first empty file locaton and store current file
    for (int i = 0; i < MAX_OPEN_FILE_CT; i++) {
        if (opened_files.current_opened_files[i].io_intf == NULL) {
            opened_files.current_opened_files[i].io_intf = *io;
            opened_files.current_opened_files[i].file_size = file_struct->byte_len;
            opened_files.current_opened_files[i].usage_flag = IN_USE;
            opened_files.current_opened_files[i].file_position = inode_position;
            opened_files.current_opened_files[i].inode = requested_inode;
            return 0;
        }
    }
    // successful fs_open
    return -EFILESYS;
}

/*
 * @brief Marks the file struct associated with io as unused.
 *
 * @specific
 * -> find the file by passed io_intf (they're all unique) and mark the file as unused
 * 
 * @param
 * -> struct io_intf* io, io_intf corresponding to the file to close
 */
void fs_close(struct io_intf* io) {
    // loop to find the file to close
    for (int i = 0; i < MAX_OPEN_FILE_CT; i++) {
        if (opened_files.current_opened_files[i].io_intf == io) {
            io->refcnt -= 1; // decrease refcnt by 1
            if (io->refcnt <= 0) {
                opened_files.current_opened_files[i].usage_flag = UNUSED;
            }
            break;
        }
    }
}

/*
 * @brief Helper function for fs ioctl. Returns the length of the file.
 *
 * @param
 * -> file_t* fd, file to return length
 * -> void* arg, auxillary argument
 * 
 * @return val
 * -> fd's length
 */
int fs_getlen(file_t* fd, void* arg) {
    *(int*)arg = fd->file_size;
    console_printf("filesize%d\n", fd->file_size);
    return *(int*)arg;
}

/*
 * @brief Helper function for fs ioctl. Returns the current position in the file.
 *
 * @param
 * -> file_t* fd, file to get position
 * -> void* arg, auxillary argument
 * 
 * @return val
 * -> fd's position(read\write position)
 */
int fs_getpos(file_t* fd, void* arg) {
    *(int*)arg = fd->file_position;
    return *(int*)arg;
}

/*
 * @brief Helper function for fs ioctl. Sets the current position in the file.
 *
 * @param
 * -> file_t* fd, file to set position
 * -> void* arg, auxillary argument, stores a pointer to the address we'd like to set position
 * 
 * @return val
 * -> fd's modified position(read\write position)
 */
int fs_setpos(file_t* fd, void* arg) {
    // fs setting position
    fd->file_position = *(int *)arg;
    return *(int *)arg;
}

/*
 * @brief Helper function for fs ioctl. Returns the block size of the filesystem.
 *
 * @param
 * -> file_t* fd, file to return block size
 * -> void* arg, auxillary argument
 * 
 * @return val
 * -> filesystem block size
 */
int fs_getblksz(file_t* fd, void* arg) {
    *(int*)arg = FS_BLKSZ;
    return *(int*)arg;
}

/*
 * @brief Performs a device-specific function based on cmd. Note, ioctl functions should return values by using arg.
 *
 * @specific
 * -> find the file by looping through file array
 * -> once found, use cmd to determine which behavior to perform
 * 
 * @param
 * -> struct io_intf* io, io_intf corresponding to the file we'd like to use
 * -> int cmd, command defined in io.h
 * -> void* arg, auxillary argument
 * 
 * @return val
 * -> determined by cmd passed or Error Code if there's an issue
 */
int fs_ioctl(struct io_intf* io, int cmd, void* arg) {
    file_t* target_file = NULL;
    // finding the file to use
    for (int i = 0; i < MAX_OPEN_FILE_CT; i++) {
        if (opened_files.current_opened_files[i].io_intf == io) {
            target_file = &opened_files.current_opened_files[i];
        }
    }

    if (target_file == NULL) {
        return -EFILESYS;
    }
    // switch on cmd to determine what to do
    switch (cmd)
    {
    case IOCTL_GETLEN:
        return fs_getlen(target_file, arg);
        break;

    case IOCTL_GETPOS:
        return fs_getpos(target_file, arg);
        break;
    
    case IOCTL_SETPOS:
        return fs_setpos(target_file, arg);
        break;

    case IOCTL_GETBLKSZ:
        return fs_getblksz(target_file, arg);
        break;

    default:
        return -EFILESYS;
        break;
    }
}

/*
 * @brief Writes n bytes from buf into the file associated with io. The size of the file should not change but only overwrite existing data. Write should also not create any new files. Updates metadata in the file struct as appropriate.
 *
 * @specific
 * -> find the inode corresponding to current file
 * -> read the inode block to find data block locations
 * -> perform write block by block(when writing larger than block size data), can perform direct write if size smaller
 * -> set file position after writing
 * 
 * @param
 * -> struct io_intf* io, io_intf corresponding to the file
 * -> const void* buf, buffer that contains the contents to write to file
 * -> unsigned long n, length to write
 * 
 * @return val
 * -> 0
 */
long fs_write(struct io_intf* io, const void* buf, unsigned long n) {
    // executing fs_write
    uint64_t write_position = ioctl(io, IOCTL_GETPOS, &write_position);
    uint64_t inode = -1;
    for (int i = 0; i < MAX_OPEN_FILE_CT; i++) {
        if (opened_files.current_opened_files[i].io_intf == io) {
            inode = opened_files.current_opened_files[i].inode;
            break;
        }
    }
    if (inode == -1) {
        return -EFILESYS;
    }

    // read the inode blocks to get data block addr
    size_t buffer_start = inode * FS_BLKSZ + FS_BLKSZ;
    system_io->ops->ctl(system_io, IOCTL_SETPOS, &buffer_start);
    // read the file_struct to perform write
    struct inode_t * file_struct = kmalloc(sizeof(struct inode_t));
    ioread_full(system_io, file_struct, FS_BLKSZ);
    if (write_position + n > file_struct->byte_len) {
        n = file_struct->byte_len - write_position;
    }

    // determine how many cycles to go through, and the remainder bytes to write after block-size writes have finished
    size_t leading = write_position % FS_BLKSZ; 
    // leading is non-zero when writing_position is no multiple of FS_BLKSZ (that current write start position in middle of a data block)
    size_t block_passed = write_position / FS_BLKSZ; 
    // block_passed records the data blocks that were completely passed (fully read/write)
    size_t leading_compensation = FS_BLKSZ - leading; 
    // leading_compensation is non-zero when leading is non-zero (FS_BLKSZ - leading)
    if (leading_compensation == FS_BLKSZ) { // leading_compensation will be set to 0 if leading is 0
        leading_compensation = 0;
    }
    // cycle and remainder both have potential of going negative, so they need to be int for negative conditions
    size_t cycle = (n - leading_compensation) / FS_BLKSZ;
    size_t remainder = (n - leading_compensation) % FS_BLKSZ;
    if (n < leading_compensation) {
        leading_compensation = 0;
        remainder = n;
        cycle = 0;
    }

    // calculate the buffer start in file system memory
    buffer_start = (file_struct->data_block_num[block_passed] + super_block.num_inodes) * FS_BLKSZ + FS_BLKSZ + leading;
    // write_buffer_idx is an aux parameter to determine which location in buf to write into file system memory
    size_t write_buffer_idx = 0;

    // set file system memory position to where we'd start writing
    system_io->ops->ctl(system_io, IOCTL_SETPOS, &buffer_start);
    for (int i = block_passed; i < DATA_BLOCK_NUM; i++) {
        // if compensation not zero, we have to start writing in the middentering vioblk readle of a previous read/write but not finished block
        if (leading_compensation != 0) {
            iowrite(system_io, buf + write_buffer_idx, leading_compensation);
            write_buffer_idx += leading_compensation;
            leading_compensation = 0;
            buffer_start = (file_struct->data_block_num[i + NEXT_DATA_BLK_IDX] + super_block.num_inodes) * FS_BLKSZ + FS_BLKSZ;
            ioctl(system_io, IOCTL_SETPOS, &buffer_start);
        } else if (cycle <= 0) { // exit when cycles finished
            // use system_io to perform write (since it has the right to directly modify file system memory)
            iowrite(system_io, buf + write_buffer_idx, remainder);
            break;
        } else {
            // use system_io to perform write (since it has the right to directly modify file system memory)
            iowrite(system_io, buf + write_buffer_idx, FS_BLKSZ);
            // update writing and receiving positions
            buffer_start = (file_struct->data_block_num[i + 1] + super_block.num_inodes) * FS_BLKSZ + FS_BLKSZ;
            ioctl(system_io, IOCTL_SETPOS, &buffer_start);
            write_buffer_idx += FS_BLKSZ;
            cycle--;
        }
    }
    write_position += n;
    // set file position (after writing)
    ioctl(io, IOCTL_SETPOS, &write_position);
    return n;
}

/*
 * @brief Reads n bytes from the file associated with io into buf. Updates metadata in the file struct as appro-priate.
 *
 * @specific
 * -> find the inode corresponding to current file
 * -> read the inode block to find data block locations
 * -> perform read block by block(when reading larger than block size data), can perform direct read if size smaller
 * -> set file position after reading
 * 
 * @param
 * -> struct io_intf* io, io_intf corresponding to the file
 * -> const void* buf, buffer that contains the contents to store read results
 * -> unsigned long n, length to read
 * 
 * @return val
 * -> 0
 */
long fs_read(struct io_intf* io, void* buf, unsigned long n) {
    uint64_t read_position = fs_ioctl(io, IOCTL_GETPOS, &read_position);
    uint64_t inode = -1;
    for (int i = 0; i < MAX_OPEN_FILE_CT; i++) {
        if (opened_files.current_opened_files[i].io_intf == io) {
            inode = opened_files.current_opened_files[i].inode;
            break;
        }
    }
    if (inode == -1) {
        return -EFILESYS;
    }

    // read the inode blocks to get data block addr
    size_t buffer_start = inode * FS_BLKSZ + FS_BLKSZ;
    ioctl(system_io, IOCTL_SETPOS, &buffer_start);

    // read the file_struct to perform write
    struct inode_t * file_struct = kmalloc(sizeof(struct inode_t));
    ioread_full(system_io, file_struct, FS_BLKSZ);
    if (read_position + n > file_struct->byte_len) {
        n = file_struct->byte_len - read_position;
    }

    // determine how many cycles to go through, and the remainder bytes to read after block-size reads have finished
    size_t leading = read_position % FS_BLKSZ; 
    // leading is non-zero when reading_position is no multiple of FS_BLKSZ (that current read start position in middle of a data block)
    size_t block_passed = read_position / FS_BLKSZ; 
    // block_passed records the data blocks that were completely passed (fully read/write)
    size_t leading_compensation = FS_BLKSZ - leading; 
    // leading_compensation is non-zero when leading is non-zero (FS_BLKSZ - leading)
    if (leading_compensation == FS_BLKSZ) { // leading_compensation will be set to 0 if leading is 0
        leading_compensation = 0;
    }
    // cycle and remainder both have potential of going negative, so they need to be int for negative conditions
    size_t cycle = (n - leading_compensation) / FS_BLKSZ;
    size_t remainder = (n - leading_compensation) % FS_BLKSZ;
    
    if (n < leading_compensation) {
        leading_compensation = 0;
        remainder = n;
        cycle = 0;
    }
    // calculate the buffer start in file system memory
    buffer_start = (file_struct->data_block_num[block_passed] + super_block.num_inodes) * FS_BLKSZ + FS_BLKSZ + leading;
    ioctl(system_io, IOCTL_SETPOS, &buffer_start);

    // read_buffer_idx is an aux parameter to determine which location in buf to read into file system memory
    size_t read_buffer_idx = 0;
    for (int i = block_passed; i < DATA_BLOCK_NUM; i++) {

        // if compensation not zero, we have to start reading in the middle of a previous read/write but not finished block
        if (leading_compensation != 0) {
            ioread_full(system_io, buf + read_buffer_idx, leading_compensation);
            read_buffer_idx += leading_compensation;
            leading_compensation = 0;
            buffer_start = (file_struct->data_block_num[i + NEXT_DATA_BLK_IDX] + super_block.num_inodes) * FS_BLKSZ + FS_BLKSZ;
            ioctl(system_io, IOCTL_SETPOS, &buffer_start);
        } else if (cycle <= 0) { // exit when cycles finished
            // use system_io to perform read (since it has the right to directly access file system memory)
            
            ioread_full(system_io, buf + read_buffer_idx, remainder);
            break;
        } else {
            // use system_io to perform read (since it has the right to directly access file system memory)
            ioread_full(system_io, buf + read_buffer_idx, FS_BLKSZ);
            // update reading and receiving positions
            buffer_start = (file_struct->data_block_num[i + 1] + super_block.num_inodes) * FS_BLKSZ + FS_BLKSZ;
            ioctl(system_io, IOCTL_SETPOS, &buffer_start);
            read_buffer_idx += FS_BLKSZ;
            cycle--;
        }
        
    }
    read_position += n;
    // set file position (after reading)
    ioctl(io, IOCTL_SETPOS, &read_position);
    // finish fs_read
    return n;
}