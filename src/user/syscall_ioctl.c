#include "syscall.h"
#include "string.h"
#include "io.h"

void main(void) {
    char filename[32];
    char c[1];
    size_t slen;
    char * greeting = "Enter file to open: ";
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
    _msgout("-------- Reading 101 Bytes from File --------");
    _fsopen(1, filename);
    char buf[101];
    _read(1, buf, 101);
    _write(0, buf, 101);
    size_t size = 20;
    _ioctl(1, IOCTL_SETPOS, &size);
    _msgout(buf);
    _msgout("-------- Reading 81 Bytes from File after Setting Pos = 20 --------");
    char buf_first_clip[81];
    _read(1, buf_first_clip, 81);
    _write(0, buf_first_clip, 81);
    size = 40;
    _ioctl(1, IOCTL_SETPOS, &size);
    _msgout(buf_first_clip);
    _msgout("-------- Reading 61 Bytes from File after Setting Pos = 40 --------");
    char buf_second_clip[61];
    _read(1, buf_second_clip, 61);
    _write(0, buf_second_clip, 61);
    size = 60;
    _ioctl(1, IOCTL_SETPOS, &size);
    _msgout(buf_second_clip);
    _msgout("-------- Reading 41 Bytes from File after Setting Pos = 60 --------");
    char buf_third_clip[41];
    _read(1, buf_third_clip, 41);
    _write(0, buf_third_clip, 41);
    size = 80;
    _ioctl(1, IOCTL_SETPOS, &size);
    _msgout(buf_third_clip);
    _msgout("-------- Reading 21 Bytes from File after Setting Pos = 20 --------");
    char buf_fourth_clip[21];
    _read(1, buf_fourth_clip, 21);
    _write(0, buf_fourth_clip, 21);
    _msgout(buf_fourth_clip);
}
