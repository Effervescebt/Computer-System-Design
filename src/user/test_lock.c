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
        _msgout("finish fork");

        // exec parent program
        message = "\n------lock parent program write 1------\n";
        slen = strlen(message);
        _write(0, message, slen);
        _msgout("parent write 1 finish");

        message = "------lock parent program write 2------\n";
        slen = strlen(message);
        _write(0, message, slen);
        _msgout("parent write 2 finish");

        message = "------lock parent program write 3------\n";
        slen = strlen(message);
        _write(0, message, slen);
        _msgout("parent write 3 finish");

        // wait for child to exit
        _msgout("parent waiting");
        _wait(1);

        // print to console, close the file, exit
        size = 0;
        _ioctl(0, IOCTL_SETPOS, &size);
        _ioctl(0, IOCTL_GETLEN, &size);
        char buf[size];
        result = _read(0, buf, size);
        _msgout("parent read file: ");
        _msgout(buf);

        _close(0);
        _msgout("parent close file");
        _exit();
    } else {
        // exec child program
        size = 200;
        _ioctl(0, IOCTL_SETPOS, &size);
        message = "\n------lock child program write 1------\n";
        slen = strlen(message);
        _write(0, message, slen);
        _msgout("child write 1 finish");

        message = "------lock child program write 2------\n";
        slen = strlen(message);
        _write(0, message, slen);
        _msgout("child write 2 finish");

        message = "------lock child program write 3------\n";
        slen = strlen(message);
        _write(0, message, slen);
        _msgout("child write 3 finish");

        // close open file and exit
        _close(0);
        _msgout("child close file");
        _exit();
    }
}
