#ifndef SHM_H
#define SHM_H

#include <shm/shm_ptr.h>

NS_BEGIN(sk)
NS_BEGIN(detail)
class size_map;
class page_heap;
NS_END(detail)

shm_ptr<void> shm_malloc(size_t bytes);
void shm_free(shm_ptr<void> ptr);

template<typename T, typename... Args>
shm_ptr<T> shm_new(Args&&... args) {
    shm_ptr<T> ptr = shm_malloc(sizeof(T));
    if (likely(ptr)) {
        T *t = ptr.get();
        new (t) T(std::forward<Args>(args)...);
    }

    return ptr;
}

template<typename T>
void shm_delete(shm_ptr<T> ptr) {
    if (likely(ptr)) {
        T *t = ptr.get();
        t->~T();
        shm_free(ptr);
    }
}

detail::size_map  *shm_size_map();
detail::page_heap *shm_page_heap();

detail::shm_address shm_allocate_metadata(size_t *bytes);
detail::shm_address shm_allocate_userdata(size_t *bytes);

void *shm_addr2ptr(detail::shm_address addr);
detail::shm_address shm_ptr2addr(const void *ptr);

int shm_init(const char *basename, bool resume_mode);
int shm_fini();

NS_END(sk)

#endif // SHM_H
