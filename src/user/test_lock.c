#include "syscall.h"
#include "string.h"
#include "io.h"

#define BLKSIZE 4096

void main(void) {
    int result;
    size_t slen;
    char message[128];
    size_t size;

    // open a file
    result = _fsopen(0, "test_lock_file");
    if (result < 0) {
        _msgout("_fsopen failed");
        _exit();
    }

    if (_fork()) {
        // exec parent program
        *message = "lock parent program write 1 ";
        slen = strlen(message);
        _write(0, message, slen);
        *message = "lock parent program write 2 ";
        slen = strlen(message);
        _write(0, message, slen);
        *message = "lock parent program write 3 ";
        slen = strlen(message);
        _write(0, message, slen);

        // wait for child to exit
        _wait(1);

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
        // exec child program
        size = BLKSIZE * 3;
        _ioctl(0, IOCTL_SETPOS, &size);
        *message = "lock child program write 1 ";
        slen = strlen(message);
        _write(0, message, slen);
        *message = "lock child program write 2 ";
        slen = strlen(message);
        _write(0, message, slen);
        *message = "lock child program write 3 ";
        slen = strlen(message);
        _write(0, message, slen);

        // close open file and exit
        _close(0);
        _exit();
    }
}
