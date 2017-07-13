#ifndef SPIN_LOCK_H
#define SPIN_LOCK_H

#include <utility/types.h>

NS_BEGIN(sk)

struct spin_lock {
    volatile int lock_;

    void init() { lock_ = 0; }

    void lock() {
        while (__sync_lock_test_and_set(&lock_, 1));
    }

    bool try_lock() {
        return __sync_lock_test_and_set(&lock_, 1) == 0;
    }

    void unlock() {
        __sync_lock_release(&lock_);
    }
};

NS_END(sk)

#endif // SPIN_LOCK_H
