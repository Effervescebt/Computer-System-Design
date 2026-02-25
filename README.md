# ECE 391 MP3 - Operating System

## Project Information

This is the MP3 (Machine Problem 3) project for ECE 391 at the University of Illinois at Urbana-Champaign (UIUC).

## Team Members - Group 61

- Haoru Li
- Xinran Hu
- Junyan Bai

## Project Structure

- `src/kern/` - Kernel source code including memory management, process management, file system, and device drivers
- `src/user/` - User space programs and utilities
- `src/util/` - Utility tools for building the file system

## Description

This project implements a fully functional RISC-V operating system kernel from scratch, featuring a complete multi-tasking environment with virtual memory support, a custom file system, and device driver infrastructure.

### Core Kernel Features

**Process and Thread Management**
- Multi-process environment with support for up to 16 concurrent processes
- Multi-threading capability with scheduling and context switching
- Process control blocks (PCB) and thread control blocks (TCB)
- Thread states management (READY, RUNNING, WAITING, STOPPED, EXITED)
- Process lifecycle management including creation, execution, and termination

**Virtual Memory Management**
- Page-based virtual memory system with page tables
- Memory protection and isolation between user and kernel space
- Support for 4KB pages with read/write/execute permissions
- Virtual address translation using RISC-V Sv39 paging
- Memory validation for user space pointers

**File System**
- Custom kernel file system (KFS) with boot block and inode structure
- Directory-based file organization with up to 63 files
- File operations: open, read, write, close
- Support for up to 32 simultaneously opened files
- Block-based I/O with 4KB block size

**ELF Executable Loader**
- ELF64 format parser and loader for RISC-V binaries
- Program header validation and loading (PT_LOAD segments)
- Virtual memory mapping for executable code and data sections
- Support for position-independent executables

**System Call Interface**
- System call dispatcher with trap handling
- User-space to kernel-space transition mechanisms
- Implemented system calls: exit, msgout, devopen, devread, devwrite, devioctl, devclose
- Secure parameter validation for system call arguments

**Device Drivers and I/O**
- VirtIO block device driver for persistent storage
- UART (NS16550a) serial port driver for console I/O
- Device manager for uniform device access
- Interrupt-driven I/O with priority levels
- Device-independent I/O interface (io_intf)

**Interrupt and Exception Handling**
- RISC-V trap handling infrastructure
- Platform-level interrupt controller (PLIC) support
- Timer interrupts for scheduling
- Exception handling for page faults, illegal instructions, etc.
- Interrupt enable/disable mechanisms with nested interrupt support

### User Space Programs

The system includes several user-space applications to demonstrate OS functionality:
- `cat` - Display file contents
- `ls` - List directory contents  
- `fib` - Fibonacci number calculator
- `greeting` - Simple greeting program
- `trek` - Star Trek game
- `zork` - Text adventure game
- `rule30` - Cellular automaton visualization
- Test programs for system calls, file operations, and synchronization

### Key Technical Details

- Architecture: RISC-V 64-bit (RV64GC)
- Memory layout: Kernel at high memory, user processes in low memory
- Synchronization: Locks for critical sections and shared resources
- Heap management: Dynamic memory allocation for kernel and processes
- Console: VGA-style text console with ANSI escape sequence support
