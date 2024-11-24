// takes in a username and says hello to that user
//

#include "syscall.h"
#include "string.h"

void main(void) {
    int result;
    result = _devopen(0, "ser", 1);
    if (result < 0) {
        _msgout("_devopen failed");
        _exit();
    }

    // get username
    char username[32];
    const char * const greeting = "Enter your username:";
    size_t slen;
    slen = strlen(greeting);
    _write(0, greeting, slen);
    _read(0, username, 32);
    slen = strlen(username);
    _write(0, username, slen);
    
    // print hello to user
    char message[128];
    snprintf(message, 128, "Hello, %s! This is enjoy life ↖(^ω^)↗ love riscv!", username);
    slen = strlen(message);
    _write(0, message, slen);
    _msgout(message);
}
