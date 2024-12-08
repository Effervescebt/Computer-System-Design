// lock.h - A sleep lock
//

#ifdef LOCK_TRACE
#define TRACE
#endif

#ifdef LOCK_DEBUG
#define DEBUG
#endif

#ifndef _LOCK_H_
#define _LOCK_H_

#include "thread.h"
#include "halt.h"
#include "console.h"
#include "intr.h"

struct lock {
    struct condition cond;
    int tid; // thread holding lock or -1
};

static inline void lock_init(struct lock * lk, const char * name);
static inline void lock_acquire(struct lock * lk);
static inline void lock_release(struct lock * lk);

// INLINE FUNCTION DEFINITIONS
//

static inline void lock_init(struct lock * lk, const char * name) {
    trace("%s(<%s:%p>", __func__, name, lk);
    condition_init(&lk->cond, name);
    lk->tid = -1;
}

/**
 * @brief:  Acquire lock for current thread.
 * @arg: the lock pointer to the lock structure that the thread attemps to acquire
 * 
 * In this function, the current thread attempts to acquire the lock. 
 * Case 1: If the lock is in unlocked state, it is changed to locked state
 * Case 2: If the lock is locked, current thread is suspended until it succeeds in acquiring the lock.
 * 
 * @notice: This function must not be called from ISR. So interrupts should be disabled here.
 * In addition, to achieve the function that current thread will suspend until the lock is unlocked.
 * So while loops is used here instead of if-else structure.
 */

static inline void lock_acquire(struct lock * lk) {
    // TODO: FIXME implement this

    trace("%s(<%s:%p>", __func__, lk->cond.name, lk);
    
    // Disable interrupt first
    int saved_intr_state = intr_disable();

    // Check lock state
    // Use while loop to ensure the current thread is suspended till the target lock is unlocked
    while (lk->tid != -1){ // If it is locked
        // Wait till the lock is in unlocked state
        condition_wait(&(lk->cond));
    }
    // Current thread acquires the lock
    lk->tid = running_thread();

    // Enable interrupt
    intr_restore(saved_intr_state);

    debug("Thread <%s:%d> released lock <%s:%p>",
        thread_name(running_thread()), running_thread(),
        lk->cond.name, lk);
}

static inline void lock_release(struct lock * lk) {
    trace("%s(<%s:%p>", __func__, lk->cond.name, lk);

    assert (lk->tid == running_thread());
    
    lk->tid = -1;
    condition_broadcast(&lk->cond);

    debug("Thread <%s:%d> released lock <%s:%p>",
        thread_name(running_thread()), running_thread(),
        lk->cond.name, lk);
}

#endif // _LOCK_H_