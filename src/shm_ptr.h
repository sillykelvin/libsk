#ifndef SHM_PTR_H
#define SHM_PTR_H

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
    typedef T* pointer;
    typedef typename detail::dereference<T>::type reference;

    u64 mid;

    shm_ptr() : mid(0) {}

    explicit shm_ptr(detail::detail_ptr ptr)
        : mid(*cast_ptr(u64, &ptr)) { assert_noeffect(sizeof(ptr) == sizeof(u64)); }

    void *__ptr() {
        if (!mid)
            return NULL;

        shm_mgr *mgr = shm_mgr::get();
        return mgr->mid2ptr(mid);
    }

    pointer get() {
        return cast_ptr(pointer, __ptr());
    }

    pointer operator->() const {
        pointer ptr = get();
        assert_noeffect(ptr);

        return ptr;
    }

    reference operator*() const {
        pointer ptr = get();
        assert_noeffect(ptr);

        return *ptr;
    }

    typedef void (shm_ptr::*unspecified_bool_type)() const;
    void unspecified_bool_true() const {}

    operator unspecified_bool_type() const { return mid ? &shm_ptr::unspecified_bool_true : 0; }
};

} // namespace sk

#endif // SHM_PTR_H
