#include "syscall.h"
#include "string.h"
#include "io.h"

void main(void) {
    char filename[32];
    char c[1];
    size_t slen;
    char * greeting = "Enter file to open: ";
    char * breakpt = "Reached\n";
    int n = 0;
    int result;

    // open serial device
    result = _devopen(0, "ser", 1);
    if (result < 0) {
        _msgout("_devopen failed");
        _exit();
    }

    // get username
    _read(0, c, 1);
    slen = strlen(greeting);
    _write(0, greeting, slen);
    for (;;) {
        _read(0, c, 1);
        int stop = 0;
        switch (*c) {
        case '\r':
            *c = '\n';
            _write(0, c, 1);
            *c = '\r';
            _write(0, c, 1);
            filename[n] = '\0';
            stop = 1;
            break;
        default:
            if (n < 32) {
                _write(0, c, 1);
                filename[n] = *c;
                n++;
            }
            else {
                greeting = "Name is too long! \n\r";
                slen = strlen(greeting);
                _write(0, greeting, slen);
                stop = 1;
            }
            break;
        }
        if (stop == 1)
            break;
    }
    _fsopen(1, filename);
    char buf[100];
    _read(1, buf, 100);
    _write(0, buf, 100);
    _msgout(breakpt);
    _ioctl(1, IOCTL_SETPOS, 0);
    _msgout(buf);
}