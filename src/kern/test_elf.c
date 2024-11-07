#include "elf.h"
#include "console.h"
#include "io.h"
#include "string.h"
#include "intr.h"
    // These global variables are required for magic number check
#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define magic_numb_1 0x7F
#define magic_numb_2 0x45
#define magic_numb_3 0x4C
#define magic_numb_4 0x46
    // These global variables are required for class check
#define EI_CLASS 4
#define ELF64 2
    // These global variables are required for little-endian or machine type check
#define EI_DATA 5
#define LITTLE_ENDIAN 5
#define MACHINE 7
#define EI_BIG_ENDIAN 2
// #include <stdio.h>
extern char _companion_f_start[];
extern char _companion_f_end[];

void test_invalid_elf(){
    struct io_lit iolit;
    struct io_intf* io = NULL;
    io = iolit_init(&iolit, _companion_f_start, _companion_f_end - _companion_f_start);
    void (*entryptr)(struct io_intf *io) = NULL;
    int result = elf_load(io, &entryptr);
    if (result == -MACHINE || result == -LITTLE_ENDIAN){
        console_printf ("Test passed: Invalid ELF rejected with error  code: %d\n", result);
    }
    else{
        console_printf ("Test failed: ELF loader accepted invalid file.\n");
    }
}

void test_valid_elf(){
    struct io_lit iolit;
    struct io_intf* io = NULL;
    io = iolit_init(&iolit, _companion_f_start, _companion_f_end - _companion_f_start);
    void (*entryptr)(struct io_intf *io) = NULL;
    int result = elf_load(io, &entryptr);
    if (result == 0){
        console_printf ("Test passed: ELF file loaded successfully\n");
        console_printf ("Entry point: %p\n", entryptr);
    }
    else{
        console_printf ("Test failed: ELF loader ELF loader error code: %d\n",result);
    }
}

int main(){
    console_printf("Running ELF loader tests...\n");
    test_invalid_elf();
    test_valid_elf();
    
    // Now reject not little endian 
    uint8_t buff[64];
    buff[EI_MAG0] = magic_numb_1;
    buff[EI_MAG1] = magic_numb_2;
    buff[EI_MAG2] = magic_numb_3;
    buff[EI_MAG3] = magic_numb_4;
    buff[EI_CLASS] = ELF64;
    buff[EI_DATA] = EI_BIG_ENDIAN;
    struct io_lit iolit;
    struct io_intf* io = NULL;
    io = iolit_init(&iolit, buff, sizeof(buff));
    void (*entryptr)(struct io_intf *io) = NULL;
    int result = elf_load(io, &entryptr);
    if (result == -MACHINE || result == -LITTLE_ENDIAN){
        console_printf ("Reject test passed: Invalid ELF rejected with error  code: %d\n", result);
    }
    else{
        console_printf ("Reject test failed: ELF loader accepted invalid file.\n");
    }
    console_printf("Tests complete.\n");
    return 0;
}

// Please remember to move "trek" from user to kern
// Please bash ./mkcomp.sh trek
// Then make run_test_elf