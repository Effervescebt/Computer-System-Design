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
    result = _fsopen(0, "test_refcnt_file");
    if (result < 0) {
        _msgout("_fsopen failed");
        _exit();
    }

    if (_fork()) {
        // exec parent program
        _close(0);
        _wait(1);
        _exit();
    } else {
        // exec child program
        _ioctl(0, IOCTL_GETLEN, &size);
        char buf[size];
        result = _read(0, buf, size);
        _msgout(buf);
        
        *message = "refcnt child program write";
        slen = strlen(message);
        _write(0, message, slen);

        // close open file and exit
        _close(0);
        _exit();
    }
}
