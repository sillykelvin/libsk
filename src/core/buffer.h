#ifndef BUFFER_H
#define BUFFER_H

#include <utility/types.h>
#include <utility/math_helper.h>
#include <utility/assert_helper.h>

NS_BEGIN(sk)

class buffer {
public:
    static const size_t DEFAULT_BUFFER_SIZE = 512;

    MAKE_NONCOPYABLE(buffer);

    buffer(size_t buffer_size = DEFAULT_BUFFER_SIZE);
    buffer(const void *data, size_t len);
    ~buffer();

    /**
     * @brief commit: moves data from writing area into reading area
     * @param len: how many bytes to move
     *
     * NOTE: this requires a preceding call prepare(x) where x >= n
     */
    void commit(size_t n);

    /**
     * @brief prepare: ensures the writing area is enough for n bytes
     * @param n: how many bytes to write
     * @return the pointer to the starting of the writing area
     *
     * NOTE: commit(x) should be called later after x bytes (x <= n)
     * data have been written into this area, the returned pointer is
     * invalidated by any member function call that might modifies the
     * buffer state, like prepare(y), consume(z)
     */
    void *prepare(size_t n);

    /**
     * @brief consume: removes data from the reading area
     * @param n: how many bytes to remove
     */
    void consume(size_t n);

    /**
     * @brief peek: get the reading area
     * @return the pointer to the starting of the reading area
     *
     * NOTE: consume(x) should be called later after x bytes
     * (x <= size()) have been handled by application, the returned
     * pointer is invalidated by any member function call that might
     * modifies the buffer state, like prepare(y), consume(z)
     */
    const void *peek() const {
        return sk::byte_offset<void>(address_, rindex_);
    }

    /**
     * @brief mutable_peek: get the reading area as mutable
     * @return the pointer to the starting of the reading area
     *
     * NOTE: the difference between this and peek() is that this
     * returns a mutable area, it's useful for those applications
     * that need to modify the area when handling the data, the
     * caller should be aware of the consequence by modifying the
     * read area, also, consume(x) should be called later after x
     * bytes (x <= size()) have been handled
     */
    void *mutable_peek() const {
        return sk::byte_offset<void>(address_, rindex_);
    }

    size_t size() const {
        sk_assert(windex_ >= rindex_);
        return windex_ - rindex_;
    }

    bool empty() const {
        sk_assert(windex_ >= rindex_);
        return windex_ == rindex_;
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
