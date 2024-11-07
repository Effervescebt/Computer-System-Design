//           vioblk.c - VirtIO serial port (console)
//          

#include "virtio.h"
#include "intr.h"
#include "halt.h"
#include "heap.h"
#include "io.h"
#include "device.h"
#include "error.h"
#include "string.h"
#include "thread.h"
#include "limits.h"

//           COMPILE-TIME PARAMETERS
//          

#define VIOBLK_IRQ_PRIO 1

//           INTERNAL CONSTANT DEFINITIONS
//          

//           VirtIO block device feature bits (number, *not* mask)

#define VIRTIO_BLK_F_SIZE_MAX       1
#define VIRTIO_BLK_F_SEG_MAX        2
#define VIRTIO_BLK_F_GEOMETRY       4
#define VIRTIO_BLK_F_RO             5
#define VIRTIO_BLK_F_BLK_SIZE       6
#define VIRTIO_BLK_F_FLUSH          9
#define VIRTIO_BLK_F_TOPOLOGY       10
#define VIRTIO_BLK_F_CONFIG_WCE     11
#define VIRTIO_BLK_F_MQ             12
#define VIRTIO_BLK_F_DISCARD        13
#define VIRTIO_BLK_F_WRITE_ZEROES   14

//           INTERNAL TYPE DEFINITIONS
//          

//           All VirtIO block device requests consist of a request header, defined below,
//           followed by data, followed by a status byte. The header is device-read-only,
//           the data may be device-read-only or device-written (depending on request
//           type), and the status byte is device-written.

struct vioblk_request_header {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

//           Request type (for vioblk_request_header)

#define VIRTIO_BLK_T_IN             0
#define VIRTIO_BLK_T_OUT            1

//           Status byte values

#define VIRTIO_BLK_S_OK         0
#define VIRTIO_BLK_S_IOERR      1
#define VIRTIO_BLK_S_UNSUPP     2

//           Main device structure.
//          
//           FIXME You may modify this structure in any way you want. It is given as a
//           hint to help you, but you may have your own (better!) way of doing things.

#define VIOBLK_SECTOR_SIZE      512
#define VIOBLK_USED_NOTF        (1 << 0)

struct vioblk_device {
    volatile struct virtio_mmio_regs * regs;
    struct io_intf io_intf;
    uint16_t instno;
    uint16_t irqno;
    int8_t opened;
    int8_t readonly;

    //           optimal block size
    uint32_t blksz;
    //           current position
    uint64_t pos;
    //           size of device in bytes
    uint64_t size;
    //           size of device in blksz blocks
    uint64_t blkcnt;

    struct {
        //           signaled from ISR
        struct condition used_updated;

        //           We use a simple scheme of one transaction at a time.

        union {
            struct virtq_avail avail;
            char _avail_filler[VIRTQ_AVAIL_SIZE(1)];
        };

        union {
            volatile struct virtq_used used;
            char _used_filler[VIRTQ_USED_SIZE(1)];
        };

        //           The first descriptor is an indirect descriptor and is the one used in
        //           the avail and used rings. The second descriptor points to the header,
        //           the third points to the data, and the fourth to the status byte.

        struct virtq_desc desc[4];
        struct vioblk_request_header req_header;
        uint8_t req_status;
    } vq;

    //           Block currently in block buffer
    uint64_t bufblkno;
    //           Block buffer
    char * blkbuf;
};

//           INTERNAL FUNCTION DECLARATIONS
//          

static int vioblk_open(struct io_intf ** ioptr, void * aux);

static void vioblk_close(struct io_intf * io);

static long vioblk_read (
    struct io_intf * restrict io,
    void * restrict buf,
    unsigned long bufsz);

static long vioblk_write (
    struct io_intf * restrict io,
    const void * restrict buf,
    unsigned long n);

static int vioblk_ioctl (
    struct io_intf * restrict io, int cmd, void * restrict arg);

static void vioblk_isr(int irqno, void * aux);

//           IOCTLs

static int vioblk_getlen(const struct vioblk_device * dev, uint64_t * lenptr);
static int vioblk_getpos(const struct vioblk_device * dev, uint64_t * posptr);
static int vioblk_setpos(struct vioblk_device * dev, const uint64_t * posptr);
static int vioblk_getblksz (
    const struct vioblk_device * dev, uint32_t * blkszptr);

//           EXPORTED FUNCTION DEFINITIONS
//          

//           Attaches a VirtIO block device. Declared and called directly from virtio.c.

/*  Initializes the virtio block device with the necessary IO operation functions and sets the required
    feature bits. Also fills out the descriptors in the virtq struct. It attaches the virtq avail and
    virtq used structs using the virtio attach virtq function. Finally, the interupt service routine
    and device are registered.
*/
void vioblk_attach(volatile struct virtio_mmio_regs * regs, int irqno) {
    //           FIXME add additional declarations here if needed

    static const struct io_ops ops = {
        .write = vioblk_write,
        .read = vioblk_read,
        .ctl = vioblk_ioctl,
        .close = vioblk_close
    };
    
    virtio_featset_t enabled_features, wanted_features, needed_features;
    struct vioblk_device * dev;
    uint_fast32_t blksz;
    int result;

    assert (regs->device_id == VIRTIO_ID_BLOCK);

    //           Signal device that we found a driver

    regs->status |= VIRTIO_STAT_DRIVER;
    //           fence o,io
    __sync_synchronize();

    //           Negotiate features. We need:
    //            - VIRTIO_F_RING_RESET and
    //            - VIRTIO_F_INDIRECT_DESC
    //           We want:
    //            - VIRTIO_BLK_F_BLK_SIZE and
    //            - VIRTIO_BLK_F_TOPOLOGY.

    virtio_featset_init(needed_features);
    virtio_featset_add(needed_features, VIRTIO_F_RING_RESET);
    virtio_featset_add(needed_features, VIRTIO_F_INDIRECT_DESC);
    virtio_featset_init(wanted_features);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_BLK_SIZE);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_TOPOLOGY);
    result = virtio_negotiate_features(regs,
        enabled_features, wanted_features, needed_features);

    if (result != 0) {
        kprintf("%p: virtio feature negotiation failed\n", regs);
        return;
    }

    //           If the device provides a block size, use it. Otherwise, use 512.

    if (virtio_featset_test(enabled_features, VIRTIO_BLK_F_BLK_SIZE))
        blksz = regs->config.blk.blk_size;
    else
        blksz = 512;

    debug("%p: virtio block device block size is %lu", regs, (long)blksz);

    //           Allocate initialize device struct

    dev = kmalloc(sizeof(struct vioblk_device) + blksz);
    memset(dev, 0, sizeof(struct vioblk_device));

    //           FIXME Finish initialization of vioblk device here

    // initialize the struct
    dev->regs = regs;
    dev->instno = regs->device_id;
    dev->irqno = irqno;
    dev->opened = 0;
    dev->blksz = blksz;
    dev->pos = 0;
    dev->size = regs->config.blk.capacity * VIOBLK_SECTOR_SIZE;
    dev->blkcnt = dev->size / blksz;
    dev->blkbuf = (void*)dev + sizeof(struct vioblk_device);
    dev->io_intf.ops = &ops;
    condition_init(&dev->vq.used_updated, "vioblk_used_updated");

    // fill out the descriptors in the virtq struct
    dev->vq.desc[0].addr = (uint64_t)&dev->vq.desc[1];
    dev->vq.desc[0].len = sizeof(dev->vq.desc[1]) * 3;
    dev->vq.desc[0].flags |= VIRTQ_DESC_F_INDIRECT;

    dev->vq.desc[1].addr = (uint64_t)&dev->vq.req_header;
    dev->vq.desc[1].len = sizeof(dev->vq.req_header);
    dev->vq.desc[1].flags |= VIRTQ_DESC_F_NEXT;
    dev->vq.desc[1].next = 1;

    dev->vq.desc[2].addr = (uint64_t)dev->blkbuf;
    dev->vq.desc[2].len = blksz;
    dev->vq.desc[2].flags |= VIRTQ_DESC_F_NEXT;
    dev->vq.desc[2].next = 2;

    dev->vq.desc[3].addr = (uint64_t)&dev->vq.req_status;
    dev->vq.desc[3].len = 1;
    dev->vq.desc[3].flags |= VIRTQ_DESC_F_WRITE;

    // attach the virtq avail and virtq used structs
    virtio_attach_virtq(dev->regs, 0, 1, (uint64_t)(void*)&dev->vq.desc, (uint64_t)(void*)&dev->vq.used, (uint64_t)(void*)&dev->vq.avail);

    // register the interrupt service routine and device
    virtio_enable_virtq(dev->regs, 0);
    __sync_synchronize();

    intr_register_isr(irqno, VIOBLK_IRQ_PRIO, vioblk_isr, dev);
    device_register("blk", &vioblk_open, dev);
 
    regs->status |= VIRTIO_STAT_DRIVER_OK;    
    //           fence o,oi
    __sync_synchronize();
}

/*  Sets the virtq avail and virtq used queues such that they are available for use. (Hint, read
    virtio.h) Enables the interupt line for the virtio device and sets necessary flags in vioblk device.
    Returns the IO operations to ioptr.
*/
int vioblk_open(struct io_intf ** ioptr, void * aux) {
    //           FIXME your code here

    struct vioblk_device * dev = aux;
    *ioptr = &dev->io_intf;

    // check if failed to open
    if (dev->opened) 
        return -EBUSY;

    // set the virtq avail and virtq used queues
    dev->vq.avail.flags = 0;
    dev->vq.avail.idx = 0;
    dev->vq.avail.ring[0] = 0;

    // enable the interrupt line for the virtio device
    dev->regs->status |= VIRTIO_STAT_DRIVER_OK;
    intr_enable_irq(dev->irqno);
    __sync_synchronize();

    // set necessary flags in vioblk device
    dev->opened = 1;

    return 0;
}

//           Must be called with interrupts enabled to ensure there are no pending
//           interrupts (ISR will not execute after closing).

/*  Resets the virtq avail and virtq used queues and sets necessary flags in vioblk device
*/
void vioblk_close(struct io_intf * io) {
    //           FIXME your code here

    struct vioblk_device * dev = (void*)io -
        offsetof(struct vioblk_device, io_intf);

    // reset the virtq avail and virtq used queues
    intr_disable_irq(dev->irqno);
    virtio_reset_virtq(dev->regs, 0);

    // set necessary flags in vioblk device
    dev->opened = 0;
}

/*  Reads bufsz number of bytes from the disk and writes them to buf. Achieves this by repeatedly
    setting the appropriate registers to request a block from the disk, waiting until the data has been
    populated in block buffer cache, and then writes that data out to buf. Thread sleeps while waiting for
    the disk to service the request. Returns the number of bytes successfully read from the disk.
*/
long vioblk_read (
    struct io_intf * restrict io,
    void * restrict buf,
    unsigned long bufsz)
{
    //           FIXME your code here
    console_printf("entering vioblk read\n");
    struct vioblk_device * dev = (void*)io -
        offsetof(struct vioblk_device, io_intf);

    if (dev->opened == 0)
        return -EBADFMT;

    // boundary check
    if (LONG_MAX < bufsz)
        bufsz = LONG_MAX;

    if (bufsz == 0)
        return 0;

    if (dev->pos + bufsz > dev->size)
        bufsz = dev->size - dev->pos;
    uint64_t old_pos = dev->pos;

    // read the block
    while (bufsz > 0) {
        int sector = dev->pos / dev->blksz;
        dev->vq.req_header.type = VIRTIO_BLK_T_IN;
        dev->vq.req_header.sector = sector;
        dev->vq.desc[2].flags |= VIRTQ_DESC_F_WRITE;
        dev->vq.avail.idx++;
        __sync_synchronize();

        // wait for the device to service the request
        int s = intr_disable();
        virtio_notify_avail(dev->regs, 0);
        condition_wait(&dev->vq.used_updated);
        intr_restore(s);

        // copy the data to the buffer
        int count = bufsz;
        int boundary = (sector + 1) * dev->blksz;
        if (dev->pos + count > boundary)
            count = boundary - dev->pos;
        memcpy(buf, dev->blkbuf + (dev->pos % dev->blksz), count);

        dev->pos += count;
        bufsz -= count;
        buf += count;
    }
    //console_printf("returning from vioblk read\n");
    //console_printf("pos: %d\n", dev->pos);

    return dev->pos - old_pos;
}

/*  Writes n number of bytes from the parameter buf to the disk. The size of the virtio device should
    not change. You should only overwrite existing data. Write should also not create any new files.
    Achieves this by filling up the block buffer cache and then setting the appropriate registers to request
    the disk write the contents of the cache to the specified block location. Thread sleeps while waiting
    for the disk to service the request. Returns the number of bytes successfully written to the disk.
*/
long vioblk_write (
    struct io_intf * restrict io,
    const void * restrict buf,
    unsigned long n)
{
    //           FIXME your code here

    struct vioblk_device * dev = (void*)io -
        offsetof(struct vioblk_device, io_intf);

    if (dev->opened == 0)
        return -EBADFMT;
    
    // boundary check
    if (LONG_MAX < n)
        n = LONG_MAX;

    if (n == 0)
        return 0;

    if (dev->pos + n > dev->size)
        n = dev->size - dev->pos;
    uint64_t old_pos = dev->pos;

    // write the block
    while (n > 0) {
        int sector = dev->pos / dev->blksz;
        int count = n;
        int boundary = (sector + 1) * dev->blksz;
        if (dev->pos + count > boundary)
            count = boundary - dev->pos;

        // read the block to be written
        // prevent overwriting
        uint64_t pos_pre = dev->pos;
        uint64_t pos_new = sector * dev->blksz;
        vioblk_ioctl(io, IOCTL_SETPOS, &pos_new);
        char * read_blk = kmalloc(dev->blksz);
        vioblk_read(io, read_blk, dev->blksz);
        vioblk_ioctl(io, IOCTL_SETPOS, &pos_pre);
        memcpy(read_blk+ (dev->pos % dev->blksz), buf, count);

        // write read_blk
        memcpy(dev->blkbuf, read_blk, dev->blksz);
        dev->vq.req_header.type = VIRTIO_BLK_T_OUT;
        dev->vq.req_header.sector = sector;
        dev->vq.avail.idx++;
        dev->vq.desc[2].flags &= ~VIRTQ_DESC_F_WRITE;
        __sync_synchronize();

        // wait for the device to service the request
        int s = intr_disable();
        virtio_notify_avail(dev->regs, 0);
        condition_wait(&dev->vq.used_updated);
        intr_restore(s);

        // update the position
        dev->pos += count;
        n -= count;
        buf += count;
    }

    return dev->pos - old_pos;
}

int vioblk_ioctl(struct io_intf * restrict io, int cmd, void * restrict arg) {
    struct vioblk_device * const dev = (void*)io -
        offsetof(struct vioblk_device, io_intf);
    
    trace("%s(cmd=%d,arg=%p)", __func__, cmd, arg);
    
    switch (cmd) {
    case IOCTL_GETLEN:
        return vioblk_getlen(dev, arg);
    case IOCTL_GETPOS:
        return vioblk_getpos(dev, arg);
    case IOCTL_SETPOS:
        return vioblk_setpos(dev, arg);
    case IOCTL_GETBLKSZ:
        return vioblk_getblksz(dev, arg);
    default:
        return -ENOTSUP;
    }
}

/*  Sets the appropriate device registers and wakes the thread up
    after waiting for the disk to finish servicing a request.
*/
void vioblk_isr(int irqno, void * aux) {
    //           FIXME your code here

    struct vioblk_device * dev = aux;
    __sync_synchronize();

    // wake up the thread
    if (dev->regs->interrupt_status & VIOBLK_USED_NOTF) {
        condition_broadcast(&dev->vq.used_updated);

        // acknowledge the interrupt
        dev->regs->interrupt_ack = dev->regs->interrupt_status;
        __sync_synchronize();
    }
}

/*  Ioctl helper function which provides the device size in bytes.
*/
int vioblk_getlen(const struct vioblk_device * dev, uint64_t * lenptr) {
    //           FIXME your code here

    *lenptr = dev->size;
    return *lenptr;
}

/*  Ioctl helper function which gets the current position in the disk which is currently being written to or
    read from.  
*/
int vioblk_getpos(const struct vioblk_device * dev, uint64_t * posptr) {
    //           FIXME your code here

    *posptr = dev->pos;
    return *posptr;
}

/*  Ioctl helper function which sets the current position in the disk which is currently being written to or
    read from.
*/
int vioblk_setpos(struct vioblk_device * dev, const uint64_t * posptr) {
    //           FIXME your code here

    dev->pos = *posptr;
    return dev->pos;
}

/*  Ioctl helper function which provides the device block size.
*/
int vioblk_getblksz (
    const struct vioblk_device * dev, uint32_t * blkszptr)
{
    //           FIXME your code here

    *blkszptr = dev->blksz;
    return *blkszptr;
}
