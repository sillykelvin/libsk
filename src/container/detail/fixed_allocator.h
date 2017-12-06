#ifndef FIXED_ALLOCATOR_H
#define FIXED_ALLOCATOR_H

#include <shm/shm.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

/**
 * fixed_allocator is an allocator that uses a fixed pool to allocate objects
 *
 * S: node size
 * N: node count
 */
template<size_t S, size_t N>
class fixed_allocator {
    static_assert(S > 0 && N > 0, "S or N underflow");
    static_assert(S >= sizeof(shm_ptr<void>), "S underflow");

    MAKE_NONCOPYABLE(fixed_allocator);

public:
    fixed_allocator()
        : head_(nullptr),
          next_(shm_ptr2addr(memory_)),
          end_(shm_ptr2addr(memory_ + S * N)),
          size_(0), peak_size_(0) {
        sk_assert(next_);
        sk_assert(next_.get() == memory_);
        sk_assert(end_.get() == memory_ + S * N);
    }

    shm_ptr<void> allocate(size_t size) {
        sk_assert(size <= S);

        if (head_) {
            shm_ptr<void> p = head_;
            head_ = *cast_ptr(shm_ptr<void>, head_.get());

            if (++size_ > peak_size_)
                peak_size_ = size_;

            return p;
        }

        if (next_ != end_) {
            shm_ptr<void> p = next_;
            next_ = shm_ptr<void>(shm_ptr2addr(char_ptr(p.get()) + S));

            if (++size_ > peak_size_)
                peak_size_ = size_;

            return p;
        }

        return nullptr;
    }

    void deallocate(const shm_ptr<void>& ptr) {
        char *p = char_ptr(ptr.get());
        assert_retnone(p >= memory_ && p < memory_ + S * N);
        assert_retnone((p - memory_) % S == 0);

        sk_assert(size_ > 0);
        --size_;
        *cast_ptr(shm_ptr<void>, p) = head_;
        head_ = ptr;
    }

    bool full() const { return size_ >= N; }
    bool empty() const { return size_ <= 0; }
    size_t size() const { return size_; }
    size_t capacity() const { return N; }
    size_t peak_size() const { return peak_size_; }

private:
    char memory_[S * N];
    shm_ptr<void> head_; // head of the free node list
    shm_ptr<void> next_; // points to the beginning of free space
    shm_ptr<void> end_;  // points to the end of memory_
    size_t size_;        // current allocated node count
    size_t peak_size_;   // max allocated node count ever
};

NS_END(detail)
NS_END(sk)

#endif // FIXED_ALLOCATOR_H
