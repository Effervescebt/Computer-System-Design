// prints the contents of a known file to the terminal
//

#include "syscall.h"
#include "string.h"
#include "io.h"

void main(void) {
    char c[1];
    int result;
    char * message = "File name: ";
    size_t slen;
    char filename[32];
    int n = 0;

    // Open ser1 device as fd=0
    result = _devopen(0, "ser", 1);
    if (result < 0) {
        _msgout("_devopen failed");
        _exit();
    }

    // open the file to be read
    _read(0, c, 1);
    slen = strlen(message);
    _write(0, message, slen);
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
                message = "File name is too long! \n\r";
                slen = strlen(message);
                _write(0, message, slen);
                stop = 1;
            }
            break;
        }
        if (stop == 1)
            break;
    }

    // open the file
    result = _fsopen(1, filename);
    if (result < 0) {
        _msgout("_fsopen failed");
        _exit();
    }

    // read the file and print it to the terminal
    size_t size = 0;
    _ioctl(1, IOCTL_GETLEN, &size);
    char buf[4096];
    result = _read(1, buf, 4096);
    if (result < 0) {
        _msgout("_read failed");
        _exit();
    }
    slen = strlen(buf);
    _write(0, buf, slen);
    _msgout(buf);

    // end the program
    message = "\n\rHit any key to end the program: ";
    slen = strlen(message);
    _write(0, message, slen);
    _read(0, c, 1);              
}
