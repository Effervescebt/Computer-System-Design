#include "syscall.h"
#include "string.h"
#include "io.h"

#define BLKSIZE 4096

void main(void) {
    int result;
    size_t slen;
    char * message;
    size_t size;

    // open a file
    result = _fsopen(0, "test_lock_file.txt");
    if (result < 0) {
        _msgout("_fsopen failed");
        _exit();
    }

    if (_fork()) {
        // exec parent program
        _msgout("finish fork");

        _close(0);
        _msgout("parent close file");

        _msgout("parent waiting");
        _wait(1);

        _exit();
    } else {
        // exec child program
        _ioctl(0, IOCTL_GETLEN, &size);
        _msgout("child finish ioctl");

        char read_buf[size];
        result = _read(0, read_buf, size);
        _msgout("child read the original file: ");
        _msgout(read_buf);
        
        size = 0;
        _ioctl(0, IOCTL_SETPOS, &size);
        _ioctl(0, IOCTL_GETLEN, &size);
        message = "-------refcnt child write------\n";
        slen = strlen(message);
        _write(0, message, slen);
        _msgout("child finish write");

        size = 0;
        _ioctl(0, IOCTL_SETPOS, &size);
        _ioctl(0, IOCTL_GETLEN, &size);
        char write_buf[size];
        result = _read(0, write_buf, size);
        _msgout("child read file after writing: ");
        _msgout(write_buf);

        // close open file and exit
        _close(0);
        _msgout("child close file");

        _exit();
    }
}
