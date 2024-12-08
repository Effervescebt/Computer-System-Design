# thrasm.s - Special functions called from thread.c
#
        
# struct thread * _thread_swtch(struct thread * resuming_thread)

# Switches from the currently running thread to another thread and returns when
# the current thread is scheduled to run again. Argument /resuming_thread/ is
# the thread to be resumed. Returns a pointer to the previously-scheduled
# thread. This function is called in thread.c. The spelling of swtch is
# historic.

        .text
        .global _thread_swtch
        .type   _thread_swtch, @function

_thread_swtch:

        # We only need to save the ra and s0 - s12 registers. Save them on
        # the stack and then save the stack pointer. Our declaration is:
        # 
        #   struct thread * _thread_swtch(struct thread * resuming_thread);
        #
        # The currently running thread is suspended and resuming_thread is
        # restored to execution. swtch returns when execution is switched back
        # to the calling thread. The return value is the previously executing
        # thread. Interrupts are enabled when swtch returns.
        #
        # tp = pointer to struct thread of current thread (to be suspended)
        # a0 = pointer to struct thread of thread to be resumed
        # 

        sd      s0, 0*8(tp)
        sd      s1, 1*8(tp)
        sd      s2, 2*8(tp)
        sd      s3, 3*8(tp)
        sd      s4, 4*8(tp)
        sd      s5, 5*8(tp)
        sd      s6, 6*8(tp)
        sd      s7, 7*8(tp)
        sd      s8, 8*8(tp)
        sd      s9, 9*8(tp)
        sd      s10, 10*8(tp)
        sd      s11, 11*8(tp)
        sd      ra, 12*8(tp)
        sd      sp, 13*8(tp)

        mv      tp, a0

        ld      sp, 13*8(tp)
        ld      ra, 12*8(tp)
        ld      s11, 11*8(tp)
        ld      s10, 10*8(tp)
        ld      s9, 9*8(tp)
        ld      s8, 8*8(tp)
        ld      s7, 7*8(tp)
        ld      s6, 6*8(tp)
        ld      s5, 5*8(tp)
        ld      s4, 4*8(tp)
        ld      s3, 3*8(tp)
        ld      s2, 2*8(tp)
        ld      s1, 1*8(tp)
        ld      s0, 0*8(tp)
                
        ret

        .global _thread_setup
        .type   _thread_setup, @function

# void _thread_setup (
#      struct thread * thr,             in a0
#      void * sp,                       in a1
#      void (*start)(void *, void *),   in a2
#      void * arg)                      in a3
#
# Sets up the initial context for a new thread. The thread will begin execution
# in /start/, receiving the five arguments passed to _thread_set after /start/.

_thread_setup:
        # Write initial register values into struct thread_context, which is the
        # first member of struct thread.
        
        sd      a1, 13*8(a0)    # Initial sp
        sd      a2, 11*8(a0)    # s11 <- start
        sd      a3, 0*8(a0)     # s0 <- arg 0
        sd      a4, 1*8(a0)     # s1 <- arg 1
        sd      a5, 2*8(a0)     # s2 <- arg 2
        sd      a6, 3*8(a0)     # s3 <- arg 3
        sd      a7, 4*8(a0)     # s4 <- arg 4

        # put address of thread entry glue into t1 and continue execution at 1f

        jal     t0, 1f

        # The glue code below is executed when we first switch into the new thread

        la      ra, thread_exit # child will return to thread_exit
        mv      a0, s0          # get arg argument to child from s0
        mv      a1, s1          # get arg argument to child from s0
        mv      fp, sp          # frame pointer = stack pointer
        jr      s11             # jump to child entry point (in s1)

1:      # Execution of _thread_setup continues here

        sd      t0, 12*8(a0)    # put address of above glue code into ra slot

        ret

        .global _thread_finish_jump
        .type   _thread_finish_jump, @function

# void __attribute__ ((noreturn)) _thread_finish_jump (
#      struct thread_stack_anchor * stack_anchor,
#      uintptr_t usp, uintptr_t upc, ...);

# @brief: This function completes the procesdure of changing to U mode
# @arg: CURTHR->stack_base: stack base of current thread
#       usp: user stack pointer
#       upc: entry point of the process
# The work of switching to user mode continues here.
# We update the sscratch.
_thread_finish_jump:
        # While in user mode, sscratch points to a struct thread_stack_anchor
        # located at the base of the stack, which contains the current thread
        # pointer and serves as our starting stack pointer.

        # TODO: FIXME your code here
        # la      ra, process_exit
        csrw    sscratch, a0     # Set sscratch to kernel stack pointer
        la      a0, _trap_entry_from_umode      # Set stvec to _trap_entry_from_umode
        csrw    stvec, a0                       #
        mv      sp, a1       # update to user stack pointer
        sret    # return to U mode


        .global _thread_finish_fork
        .type   _thread_finish_fork, @function

# extern int thread_fork_to_user (
#    struct process * child_proc, const struct trap_frame * parent_tfr);

# @brief: This function begins be saving the currently running thread. Switches to the new child process thread and
#       back to the U mode interrupt handler. It then restores the ”saved” trap frame which is actually the duplicated
#       parent trap frame. Be sure to set up the return value correctly, it will be different between the child and
#       parent process. 
# @params:
#       struct process * child_proc: child process created by sysfork
#       const struct trap_frame * parent_tfr: parent trap frame
_thread_finish_fork:
        # currently in the parent thread, we want to switch to child thread
        # store all registers from parent thread
        sd      s0, 0*8(tp)
        sd      s1, 1*8(tp)
        sd      s2, 2*8(tp)
        sd      s3, 3*8(tp)
        sd      s4, 4*8(tp)
        sd      s5, 5*8(tp)
        sd      s6, 6*8(tp)
        sd      s7, 7*8(tp)
        sd      s8, 8*8(tp)
        sd      s9, 9*8(tp)
        sd      s10, 10*8(tp)
        sd      s11, 11*8(tp)
        sd      ra, 12*8(tp)
        sd      sp, 13*8(tp)

        ld      s0, 15*8(tp)    # set s0 to parent stack base
        sub     s1, s0, sp      # set s0 to kernel size
        ld      sp, 13*8(a0)    # set sp to child stack base

        addi    s2, s1, 0       # set s2 to loop index
        addi    s3, sp, -8      # set s3 to child stack ptr
        addi    s4, s0, -8      # set s4 to parent stack ptr
        and     s5, s5, 0       # set s5 to temp register

set_child_frame:

        ld      s5, 0(s4)       # load data from parent thread stack
        sd      s5, 0(s3)       # store data into child thread stack
        addi    s3, s3, -8      # decrease both stack pointers
        addi    s4, s4, -8

        addi    s2, s2, -1      # decrease idx by 1
        bgez    s2, set_child_frame

        # return 0 if child process is running
        sd      x0, -24*8(sp)   # store 0 to a0 in trap_frame

        sub     sp, sp, s1      # set sp to child kernel stack

        ld      s0, 0*8(tp)     # restore s0
        ld      s1, 1*8(tp)
        ld      s2, 2*8(tp)
        ld      s3, 3*8(tp)
        ld      s4, 4*8(tp)
        ld      s5, 5*8(tp)

        mv      tp, a0  # switch to child thread

        ret

# Statically allocated stack for the idle thread.

        .section        .data.stack, "wa", @progbits
        .balign          16
        
        .equ            IDLE_STACK_SIZE, 4096

        .global         _idle_stack_lowest
        .type           _idle_stack_lowest, @object
        .size           _idle_stack_lowest, IDLE_STACK_SIZE

        .global         _idle_stack_anchor
        .type           _idle_stack_anchor, @object
        .size           _idle_stack_anchor, 2*8

_idle_stack_lowest:
        .fill   IDLE_STACK_SIZE, 1, 0xA5

_idle_stack_anchor:
        .global idle_thread # from thread.c
        .dword  idle_thread
        .fill   8
        .end
        