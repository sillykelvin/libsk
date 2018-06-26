#ifndef LOCK_GUARD_H
#define LOCK_GUARD_H

#include <utility/types.h>

NS_BEGIN(sk)

/**
 * L is a lock type, should have the following function:
 *   - void lock()
 *   - void unlock()
 */
template<typename L>
class lock_guard {
public:
    MAKE_NONCOPYABLE(lock_guard);

    explicit lock_guard(L& lock) : lock_(&lock) {
        lock_->lock();
    }

    ~lock_guard() {
        if (lock_) lock_->unlock();
    }

private:
    L *lock_;
};

NS_END(sk)

#endif // LOCK_GUARD_H
