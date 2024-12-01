// test syscall function
//

#include "syscall.h"
#include "string.h"
#include "io.h"

void main(void) {
    char c[1];
    size_t slen;
    int result;

    // test msgout
    _msgout("msgout passed");

    // test devopen
    result = _devopen(0, "ser", 1);
    if (result < 0) {
        _msgout("_devopen failed");
        _exit();
    }
    _msgout("devopen passed");

    // test fsopen
    result = _fsopen(1, "trek");
    if (result < 0) {
        _msgout("_fsopen failed");
        _exit();
    }
    _msgout("fsopen passed");

    //test ioctl
    result = _ioctl(1, IOCTL_GETLEN, &slen);
    if (result < 0) {
        _msgout("_ioctl failed");
        _exit();
    }
    _msgout("ioctl passed");


    //test close
    result = _close(1);
    if (result < 0) {
        _msgout("_close failed");
        _exit();
    }
    _msgout("close passed");

    //test read
    result = _read(0, c, 1);
    if (result < 0) {
        _msgout("_read failed");
        _exit();
    }
    _msgout("read passed");

    //test write
    slen = strlen("write passed\n\r");
    result = _write(0, "write passed\n\r", slen);
    if (result < 0) {
        _msgout("_write failed");
        _exit();
    }
    _msgout("write passed");

    //test exit
    _read(0, c, 1);
    _exit();
    _msgout("exit failed");
}
