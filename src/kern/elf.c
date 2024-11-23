#include "elf.h"
// Include fs.h to ensure fs_read and fs_ioctl can work normally
#include "fs.h"
#include "io.h"
#include "console.h"
#include "string.h"
#include "intr.h"

// Now define some global variables 
#define EI_NIDENT 16
    // Here are error codes
#define READ_FAILURE 1
#define DATA_SHORTAGE 2
#define MAGIC 3
#define CLASS 4
#define LITTLE_ENDIAN 5
#define ABI 6
#define MACHINE 7
#define PROG_READ_FAILURE 8
#define PROG_ADDR 9
#define PROG_SEC_READ 10

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
    // These global variables are required for data check
#define EI_DATA 5
#define EI_ENDIAN 1
    // THese global variables are required for ABI check
#define EI_ABI 7
#define System_V 0x00
    // These global variables are required for machine check
// #define EI_MACHINE 12
#define RISCV 0xF3

    // This global variable is required for Entry type check
#define PT_LOAD 0x01

#define PT_X 1
#define PT_W 2
#define PT_R 4

    // This global variable is required for program header address check
#define addr_upper_bound 0x81000000
#define addr_lower_bound 0x80100000
    // This global variable is required for getting to program header
#define IOCTL_SETPOS 4

// Here are some common elf definitions for 64-bit architectures
typedef uint64_t	Elf64_Addr;
typedef uint16_t	Elf64_Half;
typedef uint64_t	Elf64_Off;
typedef int32_t		Elf64_Sword;
typedef int64_t		Elf64_Sxword;
typedef uint32_t	Elf64_Word;
typedef uint64_t	Elf64_Lword;
typedef uint64_t	Elf64_Xword;

// Then it is our elf header.
// Elf header stores metadata about the file
// It should always be at offset 0 of the file
typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    Elf64_Addr     e_entry; // Entry point (virtual addr where system first transfers control)
    Elf64_Off      e_phoff; // Program header file offset (in bytes)
    Elf64_Off      e_shoff; // Section header file offset (in bytes)
    uint32_t      e_flags;
    uint16_t      e_ehsize; // Size of ELF header (in bytes)
    uint16_t      e_phentsize; // Size of program header 
    uint16_t      e_phnum; // Number of program header entries
    uint16_t      e_shentsize; // Size of section header entry
    uint16_t      e_shnum; // Number of section header entries
    uint16_t      e_shstrndx;
} Elf64_Ehdr;

// This is program header. It contains the info that the system needs to prepare program to execution
// It is found at file offset e_phoff, and consists of e_phnum entries, each with size e_phentsize.
typedef struct {
	Elf64_Word	p_type;		/* Entry type. */
	Elf64_Word	p_flags;	/* Access permission flags. */
	Elf64_Off	p_offset;	/* File offset of contents. */
	Elf64_Addr	p_vaddr;	/* Virtual address in memory image. */
	Elf64_Addr	p_paddr;	/* Physical address */
	Elf64_Xword	p_filesz;	/* Size of contents in file. */
	Elf64_Xword	p_memsz;	/* Size of contents in memory. */
	Elf64_Xword	p_align;	/* Alignment in memory and file. */
} Elf64_Phdr;

uint_fast8_t flag_convert(uint32_t flags) {
    uint_fast8_t pte_flags = 0;
    if (flags & PT_X) {
        pte_flags |= PTE_X; 
    }
    if (flags & PT_W) {
        pte_flags |= PTE_W;
    }
    if (flags & PT_R) {
        pte_flags |= PTE_R;
    }
    return pte_flags;
}

// Note that ELF load function should read the elf header and process the program headers.
// The loader should only load prorgam header entries of type pt_load
// Each struct io intf contains a pointer to a struct io ops, which in turn contains pointers to close, open, read, and write functions 
// specific to each device driver

/* @brief: This function mainly loads an executable ELF file into memory and returns the entry point
@arg: arg1: io interface from which to load the elf 
    arg2: pointer to void(*entry)(struct io_intf *io), 
    which is a function pointer elf_load fills in w/ the address of the entry point

This function first check the validity of the elf header to make sure it is actually executable file.
Then iterate over all the program headers. Remember to check the type of program header first and then the address range
Then we read the contents within individual program header and set the entry point (in elf header!!!) so that we can start 
    execute the program from the entry pointer

@return: int: 0 if successful, negative values if an error occurs
*/
int elf_load(struct io_intf *io, void (**entryptr)(void)){
    // We should first check the validity of the passed ELF file
    // validating checklist: Magic, Class, Data, ABI, Machine

    // First create an empty buf and use io_read
    // size of elf header should be 64 bit
    Elf64_Ehdr elf_header; // Initialize the elf header
    // char* buffer = NULL; // Initialize the empty buffer
    // console_printf("elf header initialize successfully\n");
    long result = ioread_full(io, &elf_header, sizeof(Elf64_Ehdr));
    // console_printf("Result of ioread: %ld\n", result);
    unsigned char *buffer = elf_header.e_ident;
    // console_printf("checkpoint 0 reached, Entry point address: %p\n",(void *)elf_header.e_entry);
    // Case 1: Failure in reading
    if (result < 0 || buffer == NULL){
        // failure in data reading
        return -READ_FAILURE;
    }
    //Case 2: insufficient data
    if (result < sizeof(Elf64_Ehdr)){
        // insufficient read data
        return -DATA_SHORTAGE;
    }

    // Case 3: magic check
    if (buffer[EI_MAG0] != magic_numb_1 || buffer[EI_MAG1] != magic_numb_2 ||
        buffer[EI_MAG2] != magic_numb_3 || buffer[EI_MAG3] != magic_numb_4){
            // failure in magic check
            return -MAGIC;
        }

    // Case 4: class check
    if (buffer[EI_CLASS] != ELF64){
        // incorrect 64-bit format
        return -CLASS;
    }

    // Case 5: data check
    if(buffer[EI_DATA] != EI_ENDIAN){
        // -5 indicates incorrect endian (should be little endian)
        return -LITTLE_ENDIAN;
    }

    // Case 6: ABI check
    if (buffer[EI_ABI] != System_V){
        // incorrect ABI
        return -ABI;
    }

    // Case 7: machine check
    if(elf_header.e_machine != RISCV){
        // incorrect machine
        return -MACHINE;
    }
    // Now we have reached the end of valid check (at least I think so)
    // Next step would be processing the program headers
    // Notice that The loader should only load program header entries of type PT_LOAD
    // Now iterate all the program header entries
    for (int i = 0; i < elf_header.e_phnum; i++){
        // Initialize a new pointer of program header
        Elf64_Phdr elf_phdr;
        ioseek(io, elf_header.e_phoff + i * elf_header.e_phentsize);
        // Read data into phdr
        long result_2 = ioread_full(io, (void *)&elf_phdr, sizeof(Elf64_Phdr));
        if (result_2 < 0){
            // read failure in program header
            return -PROG_READ_FAILURE;
        }
        // check p_type here given we only load PY_LOAD
        if(elf_phdr.p_type == PT_LOAD){
            // check address 
            if (elf_phdr.p_vaddr < USER_START_VMA || elf_phdr.p_vaddr + elf_phdr.p_filesz > USER_END_VMA) {
                return -PROG_ADDR;
            }
            // struct pte* elf_entry = walk_pt(active_space_root() , elf_phdr.p_vaddr, 1);
           
            // kprintf("allocating size of %d\n", elf_phdr.p_filesz);
            uint_fast8_t pte_flags = flag_convert(elf_phdr.p_flags);
            uintptr_t allocated_page = (uintptr_t)(memory_alloc_and_map_range(elf_phdr.p_vaddr, elf_phdr.p_filesz, PTE_R | PTE_W));            
            
            // Now the address is within valid range
            // First we get position
            ioseek(io,elf_phdr.p_offset);
            // kprintf("try to allocate to %x\r\n", elf_phdr.p_vaddr);
            long prog_result = ioread_full(io, (void *)elf_phdr.p_vaddr, elf_phdr.p_filesz);
            memory_set_range_flags(elf_phdr.p_vaddr, elf_phdr.p_filesz, pte_flags | PTE_U);
            if (prog_result < elf_phdr.p_filesz){
                //  failure in program seg load
                return -PROG_SEC_READ;
            }
        }
    }
    // Now we need to move read program seg to entry point
    // console_printf("checkpoint 1 reached\n");
    *entryptr = (void*)(struct io_intf*)elf_header.e_entry;   
    // console_printf("checkpoint 2 reached, Entry point address: %p \n", *entryptr);
    return 0;
}