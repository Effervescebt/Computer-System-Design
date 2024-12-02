#include "syscall.h"
#include "string.h"

typedef unsigned long uintptr_t;

void main(void) {
    uintptr_t kern_addr = 0x80003000;
    //*((volatile uintptr_t *)kern_addr) = 1000;
    size_t result = *((volatile uintptr_t *)kern_addr);
    if (result) {}
}