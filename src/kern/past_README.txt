TESTS FROM PAST CHECKPOINTS

kern/vir_memory_test:
    run by make mem_test.elf & make run-mem_test
    virtual memory test covering basic page assignment, mapping, read/write, and read/write without permission (faults after executing)

kern/vir_memory_bugtest:
    run by make mem_test_buggy.elf & make run-buggy
    virtual memory test for executing file without execution permission (not PTE_X set), causes fault

user/cat:
    executable name in kfs.raw: cat
    preview of file (first 4096 bytes)

user/greeting:
    executable name in kfs.raw: cat
    fun user program that outputs greeting message
    screen to serial port, press enter and enter username

user/syscall_ioctl:
    executable name in kfs.raw: syscall_ioctl
    tests if syscall_ioctl's setpos works as intended
    screen to serial port, press enter and enter file name (numbers.txt)

user/test_syscall:
    executable name in kfs.raw: test_syscall
    general test for syscall, tests for successful function execution

user/user_access_kern:
    executable in kfs.raw: user_access_kern
    test for user program accessing kernel page, fires fault

kern/vioblk_test
kern/test_fs
kern/test_elf
    no longer runnable after memory and heap files modified by cp2
    yet kfs, elf, and vioblk functionalities are 100% satisfied as cp3 & cp2 all runs as desired