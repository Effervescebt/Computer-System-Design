// a number guessing game where your program says higher or lower
// for a guess until a target is reached
//

#include "syscall.h"
#include "string.h"
#include <stdlib.h>
#include <time.h>

void main(void) {

    // generate target number
    int max = 100;
    int min = 0;
    srand(time(NULL));
    int target = rand() % (max - min + 1) + min;

    // initialize guess
    int guess = 0;
    char buf[32];
    char *msg = "Guess a number between 0 and 100: ";
    char *high = "Too high!";
    char *low = "Too low!";
    char *correct = "Correct!";

    // loop until the guess is correct
    while (guess != target) {
        _msgout(msg);
        _msgin(buf, 32);
        guess = atoi(buf);

        if (guess > target) {
            _msgout(high);
        } else if (guess < target) {
            _msgout(low);
        } else {
            _msgout(correct);
        }
    }    
}
