// takes in a username and says hello to that user
//

#include "syscall.h"
#include "string.h"

void main(void) {

    // get username
    char username[32];
    _msgout("Enter your username:");
    _msgin(username, strlen(username));

    // print hello to user
    char message[128];
    snprintf(message, 128, "Hello, %s! This is enjoy life ↖(^ω^)↗ love riscv!", username);
    _msgout(message);
}
