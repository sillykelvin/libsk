#include <core/buffer.h>

NS_BEGIN(sk)

buffer::buffer(size_t buffer_size) {
    capacity_ = buffer_size;
    address_ = malloc(capacity_);
    sk_assert(address_);

    rindex_ = 0;
    windex_ = 0;
}

buffer::buffer(const void *data, size_t len) {
    capacity_ = len;
    address_ = malloc(capacity_);
    sk_assert(address_);

    rindex_ = 0;
    windex_ = 0;

    memcpy(address_, data, len);
    windex_ += len;
}

buffer::~buffer() {
    if (address_) {
        free(address_);
        address_ = nullptr;
    }

    capacity_ = 0;
    rindex_ = 0;
    windex_ = 0;
}

void buffer::commit(size_t n) {
    sk_assert(capacity_ >= windex_);
    sk_assert(windex_ + n <= capacity_);
    windex_ += n;
}

void *buffer::prepare(size_t n) {
    ensure_space(n);
    return sk::byte_offset<void>(address_, windex_);
}

void buffer::consume(size_t n) {
    size_t sz = size();
    sk_assert(n <= sz);
    rindex_ += n;

    if (n >= sz) {
        rindex_ = 0;
        windex_ = 0;
    }
}

void buffer::ensure_space(size_t n) {
    sk_assert(capacity_ >= windex_);
    size_t available = capacity_ - windex_;
    if (available >= n) return;

    // there is enough space inside buffer, just move the data
    if (rindex_ + available >= n) {
        sk_assert(rindex_ > 0);

        size_t sz = size();
        if (sz > 0)
            memmove(address_, sk::byte_offset<void>(address_, rindex_), sz);

        rindex_ = 0;
        windex_ = rindex_ + sz;

        return;
    }

    size_t sz = size();
    size_t cap = windex_ + n;
    void *addr = malloc(cap);
    sk_assert(addr);

    if (sz > 0)
        memcpy(addr, sk::byte_offset<void>(address_, rindex_), sz);

    rindex_ = 0;
    windex_ = rindex_ + sz;
    capacity_ = cap;
    free(address_);
    address_ = addr;
}

NS_END(sk)
