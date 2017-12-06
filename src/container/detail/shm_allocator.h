#ifndef SHM_ALLOCATOR_H
#define SHM_ALLOCATOR_H

#include <shm/shm.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

/**
 * shm_allocator is an allocator that allocates objects in shm
 */
class shm_allocator {
    MAKE_NONCOPYABLE(shm_allocator);

public:
    shm_allocator() : size_(0), peak_size_(0) {}

    shm_ptr<void> allocate(size_t size) {
        shm_ptr<void> ptr = shm_malloc(size);
        if (ptr && ++size_ > peak_size_) peak_size_ = size_;

        return ptr;
    }

    void deallocate(const shm_ptr<void>& ptr) {
        sk_assert(size_ > 0);
        --size_;
        shm_free(ptr);
    }

    bool full() const { return false; }
    bool empty() const { return size_ <= 0; }
    size_t size() const { return size_; }
    size_t capacity() const { return std::numeric_limits<size_t>::max(); }
    size_t peak_size() const { return peak_size_; }

private:
    size_t size_;      // current allocated node count
    size_t peak_size_; // max allocated node count ever
};

NS_END(detail)
NS_END(sk)

#endif // SHM_ALLOCATOR_H
