#ifndef SHM_PTR_H
#define SHM_PTR_H

namespace sk {

template<typename T>
struct shm_ptr {
    typedef T* pointer;
    typedef T& reference;

    u64 mid;

    shm_ptr() : mid(0) {}

    template<typename U>
    shm_ptr(U ptr) : mid(*cast_ptr(u64, &ptr)) { assert_noeffect(sizeof(ptr) == sizeof(u64)); }

    void *__ptr() {
        if (!mid)
            return NULL;

        shm_mgr *mgr = shm_mgr::get();
        return mgr->mid2ptr(mid);
    }

    pointer get() {
        return cast_ptr(pointer, __ptr());
    }
};

} // namespace sk

#endif // SHM_PTR_H
