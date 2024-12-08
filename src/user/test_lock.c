#include "syscall.h"
#include "string.h"
#include "io.h"

#define BLKSIZE 4096

void main(void) {
    int result;
    size_t slen;
    char* message;
    size_t size;

    // open a file
    result = _fsopen(0, "test_lock_file.txt");
    if (result < 0) {
        _msgout("_fsopen failed");
        _exit();
    }

    // // Open ser device as fd=0
    // result = _devopen(0, "ser", 1);
    // if (result < 0) {
    //     _msgout("_devopen failed");
    //     _exit();
    // }

    _msgout("ready to enter fork");
    if (_fork()) {
        _msgout("enter parent");
        // exec parent program
        message = "lock parent program write 1 ";
        slen = strlen(message);
        _msgout("parent write 1 ready");
        _write(0, message, slen);
        _msgout("parent write 1 finished");

        message = "lock parent program write 2 ";
        slen = strlen(message);
        _write(0, message, slen);
        _msgout("parent write 2 finished");

        message = "lock parent program write 3 ";
        slen = strlen(message);
        _write(0, message, slen);
        _msgout("parent write 3 finished");

        // wait for child to exit
        _wait(0);

        // print to console, close the file, exit
        size = 0;
        _ioctl(0, IOCTL_SETPOS, &size);
        _ioctl(0, IOCTL_GETLEN, &size);
        char buf[size];
        result = _read(0, buf, size);
        _msgout(buf);
        _close(0);
        _exit();
    } else {
        _msgout("enter child");
        // exec child program
        size = BLKSIZE * 3;
        _ioctl(0, IOCTL_SETPOS, &size);
        message = "lock child program write 1 ";
        slen = strlen(message);
        _write(0, message, slen);
        message = "lock child program write 2 ";
        slen = strlen(message);
        _write(0, message, slen);
        message = "lock child program write 3 ";
        slen = strlen(message);
        _write(0, message, slen);

        // close open file and exit
        _close(0);
        _exit();
    }
}
