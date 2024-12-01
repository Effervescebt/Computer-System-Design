// a number guessing game where your program says higher or lower
// for a guess until a target is reached
//

#include "syscall.h"
#include "string.h"
// #include <stdio.h>
#include <stdlib.h>
// #include <time.h>

void main(void) {
    char c[1];
    size_t slen;
    int n = 0;
    int result;

    // open serial device
    result = _devopen(0, "ser", 1);
    if (result < 0) {
        _msgout("_devopen failed");
        _exit();
    }

    // initialize guess
    int guess = 0;
    char buf[3];
    char * msg = "Guess a number between 0 and 100: ";
    char * high = "Too high!";
    char * low = "Too low!";
    char * correct = "Correct!";

    // generate target number
    // int max = 100;
    // int min = 0;
    // srand(time(NULL));
    // int target = rand() % (max - min + 1) + min;
    int target = 53;

    // loop until the guess is correct
    while (guess != target) {
        slen = strlen(msg);
        _write(0, msg, slen);

        for (;;) {
            _read(0, c, 1);
            int stop = 0;
            switch (*c) {
            case '\r':
                *c = '\n';
                _write(0, c, 1);
                *c = '\r';
                _write(0, c, 1);
                buf[n] = '\0';
                stop = 1;
                break;
            default:
                if (n < 3) {
                    _write(0, c, 1);
                    buf[n] = *c;
                    n++;
                }
                else {
                    msg = "Your number is out of range! \n\r";
                    slen = strlen(msg);
                    _write(0, msg, slen);
                    stop = 1;
                }
                break;
            }
            if (stop == 1)
                break;
        }        
        
        guess = atoi(buf);
        if (guess > target) {
            slen = strlen(high);
            _write(0, high, slen);
        } else if (guess < target) {
            slen = strlen(low);
            _write(0, low, slen);
        } else {
            slen = strlen(correct);
            _write(0, correct, slen);
        }
    }

    // end the program
    msg = "Hit any key to end the game: ";
    slen = strlen(msg);
    _write(0, msg, slen);
    for (;;) {
        _read(0, c, 1);
        break;
    }
}
