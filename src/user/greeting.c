// takes in a username and says hello to that user
//

#include "syscall.h"
#include "string.h"

void main(void) {
    char username[32];
    char c[1];
    size_t slen;
    char * greeting = "Enter your username: ";
    int n = 0;
    char message[128];
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
            username[n] = '\0';
            stop = 1;
            break;
        default:
            if (n < 32) {
                _write(0, c, 1);
                username[n] = *c;
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
    
    // print hello to user
    snprintf(message, 128, "Hello, %s! This is enjoy life ↖(^ω^)↗ love riscv!\n\r", username);
    slen = strlen(message);
    _write(0, message, slen);

    // end the program
    greeting = "Hit any key to end the program: ";
    slen = strlen(greeting);
    _write(0, greeting, slen);
    _read(0, c, 1);
}
