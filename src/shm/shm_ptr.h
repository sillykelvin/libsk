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
    typedef T*                                    pointer;
    typedef typename detail::dereference<T>::type reference;

    offset_t offset;

    shm_ptr() : offset(OFFSET_NULL) {}

    explicit shm_ptr(offset_t offset) : offset(offset) {}

    template<typename U>
    shm_ptr(shm_ptr<U> ptr) : offset(ptr.offset) {}

    template<typename U>
    shm_ptr& operator=(const shm_ptr<U>& ptr) {
        this->offset = ptr.offset;
        return *this;
    }

    template<typename U>
    bool operator==(const shm_ptr<U>& ptr) const {
        return this->offset == ptr.offset;
    }

    template<typename U>
    bool operator!=(const shm_ptr<U>& ptr) const {
        return this->offset != ptr.offset;
    }

    void *__ptr() const {
        if (offset == OFFSET_NULL)
            return NULL;

        shm_mgr *mgr = shm_mgr::get();
        return mgr->offset2ptr(offset);
    }

    void *__ptr() {
        if (offset == OFFSET_NULL)
            return NULL;

        shm_mgr *mgr = shm_mgr::get();
        return mgr->offset2ptr(offset);
    }

    pointer get() const {
        return cast_ptr(T, __ptr());
    }

    pointer get() {
        return cast_ptr(T, __ptr());
    }

    pointer operator->() const {
        pointer ptr = get();
        assert_noeffect(ptr);

        return ptr;
    }

    pointer operator->() {
        pointer ptr = get();
        assert_noeffect(ptr);

        return ptr;
    }

    reference operator*() const {
        pointer ptr = get();
        assert_noeffect(ptr);

        return *ptr;
    }

    reference operator*() {
        pointer ptr = get();
        assert_noeffect(ptr);

        return *ptr;
    }

    typedef void (shm_ptr::*unspecified_bool_type)() const;
    void unspecified_bool_true() const {}

    operator unspecified_bool_type() const {
        return offset != OFFSET_NULL ? &shm_ptr::unspecified_bool_true : 0;
    }
};

} // namespace sk

#endif // SHM_PTR_H
