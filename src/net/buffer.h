#ifndef BUFFER_H
#define BUFFER_H

#include "utility/types.h"
#include "utility/math_helper.h"
#include "utility/assert_helper.h"

NS_BEGIN(sk)

class buffer {
public:
    MAKE_NONCOPYABLE(buffer);

    static const size_t default_buffer_size = 512;

    buffer(size_t buffer_size = default_buffer_size);
    ~buffer();

    void append(const void *data, size_t len);
    void consume(size_t len);

    const char *peek() const {
        return sk::byte_offset<char>(address_, rindex_);
    }

    size_t size() const {
        sk_assert(windex_ >= rindex_);
        return windex_ - rindex_;
    }

private:
    void ensure_space(size_t len);

private:
    void *address_;
    size_t capacity_;
    size_t rindex_;
    size_t windex_;
};

NS_END(sk)

#endif // BUFFER_H
