#ifndef OFFSET_PTR_H
#define OFFSET_PTR_H

#include "utility/types.h"
#include "shm/shm_mgr.h"
#include "utility/assert_helper.h"

namespace sk {
namespace detail {

template<typename T>
struct offset_ptr {
    offset_t offset;

    offset_ptr() : offset(OFFSET_NULL) {}
    explicit offset_ptr(offset_t offset) : offset(offset) {}

    bool operator==(const offset_ptr& that) const { return this->offset == that.offset; }
    bool operator!=(const offset_ptr& that) const { return this->offset != that.offset; }

    void operator=(const offset_ptr& that) { this->offset = that.offset; }

    template<typename U>
    offset_ptr<U> as() { return offset_ptr<U>(offset); }

    void *__ptr() const {
        check_retval(offset != OFFSET_NULL, NULL);
        return shm_mgr::get()->offset2ptr(offset);
    }

    void *__ptr() {
        check_retval(offset != OFFSET_NULL, NULL);
        return shm_mgr::get()->offset2ptr(offset);
    }

    T *get() const { return cast_ptr(T, __ptr()); }
    T *get() { return cast_ptr(T, __ptr()); }

    explicit operator bool() const { return offset != OFFSET_NULL; }

    void set_null() { offset = OFFSET_NULL; }
    static offset_ptr null() { return offset_ptr(OFFSET_NULL); }
};

} // namespace detail
} // namespace sk

#endif // OFFSET_PTR_H
