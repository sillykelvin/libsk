#ifndef SHM_PTR_H
#define SHM_PTR_H

#include "utility/types.h"
#include "utility/config.h"
#include "utility/assert_helper.h"
#include "shm/shm_mgr.h"

namespace sk {
namespace detail {

template<typename T>
struct dereference {
    typedef T& type;
};

template<>
struct dereference<void> {
    typedef void type;
};

} // namespace detail

template<typename T>
struct shm_ptr {
    typedef T*                                    pointer;
    typedef typename detail::dereference<T>::type reference;

    u64 mid;

    shm_ptr() : mid(MID_NULL) {}

    explicit shm_ptr(u64 mid) : mid(mid) {}

    template<typename U>
    shm_ptr(shm_ptr<U> ptr) : mid(ptr.mid) {}

    template<typename U>
    shm_ptr& operator=(const shm_ptr<U>& ptr) {
        this->mid = ptr.mid;
        return *this;
    }

    template<typename U>
    bool operator==(const shm_ptr<U>& ptr) const {
        return this->mid == ptr.mid;
    }

    template<typename U>
    bool operator!=(const shm_ptr<U>& ptr) const {
        return this->mid != ptr.mid;
    }

    void *__ptr() const {
        check_retval(mid != MID_NULL, NULL);
        return shm_mgr::get()->mid2ptr(mid);
    }

    void *__ptr() {
        check_retval(mid != MID_NULL, NULL);
        return shm_mgr::get()->mid2ptr(mid);
    }

    pointer get() const {
        return cast_ptr(T, __ptr());
    }

    pointer get() {
        return cast_ptr(T, __ptr());
    }

    pointer operator->() const {
        pointer ptr = get();
        sk_assert(ptr);

        return ptr;
    }

    pointer operator->() {
        pointer ptr = get();
        sk_assert(ptr);

        return ptr;
    }

    reference operator*() const {
        pointer ptr = get();
        sk_assert(ptr);

        return *ptr;
    }

    reference operator*() {
        pointer ptr = get();
        sk_assert(ptr);

        return *ptr;
    }

    explicit operator bool() const { return mid != MID_NULL; }
};

} // namespace sk

#endif // SHM_PTR_H
