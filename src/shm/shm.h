#ifndef SHM_H
#define SHM_H

#include <shm/shm_ptr.h>

NS_BEGIN(sk)
NS_BEGIN(detail)
class size_map;
class page_heap;
NS_END(detail)

/*
 * the reserved shm singletons
 */
enum shm_singleton_id {
    SHM_SINGLETON_SHM_TIMER_MGR = 0,
    SHM_SINGLETON_RESERVED_MAX
};

shm_ptr<void> shm_malloc(size_t bytes);
void shm_free(shm_ptr<void> ptr);

bool shm_has_singleton(int id);
shm_ptr<void> shm_get_singleton(int id, size_t bytes, bool *first_call);
void shm_free_singleton(int id);

template<typename T, typename... Args>
shm_ptr<T> shm_new(Args&&... args) {
    shm_ptr<void> ptr = shm_malloc(sizeof(T));
    if (likely(ptr)) new (ptr.get()) T(std::forward<Args>(args)...);

    return shm_ptr<T>(ptr.address());
}

template<typename T>
void shm_delete(shm_ptr<T> ptr) {
    if (likely(ptr)) {
        T *t = ptr.get();
        t->~T();
        shm_free(shm_ptr<void>(ptr.address()));
    }
}

template<typename T, typename... Args>
shm_ptr<T> shm_array_new(size_t n, Args&&... args) {
    shm_ptr<void> ptr = shm_malloc(sizeof(T) * n);
    if (likely(ptr)) {
        T *array = cast_ptr(T, ptr.get());
        for (size_t i = 0; i < n; ++i)
            new (array + i) T(std::forward<Args>(args)...);
    }

    return shm_ptr<T>(ptr.address());
}

template<typename T>
void shm_array_delete(shm_ptr<T> ptr, size_t n) {
    if (likely(ptr)) {
        T *array = ptr.get();
        for (size_t i = 0; i < n; ++i)
            (array + i)->~T();

        shm_free(shm_ptr<void>(ptr.address()));
    }
}

/**
 * @brief shm_get_singleton returns a singleton instance in shm
 * @param id: the id represents an instance
 * @param args: constructor arguments for type T
 * @return the instance
 *
 * NOTE: for user level singletons, the id should be bigger than
 * SHM_SINGLETON_RESERVED_MAX, as there are some ids are reserved
 * for internal usage
 */
template<typename T, typename... Args>
shm_ptr<T> shm_get_singleton(int id, Args&&... args) {
    bool first_call = false;
    shm_ptr<void> ptr = shm_get_singleton(id, sizeof(T), &first_call);
    if (ptr && first_call) new (ptr.get()) T(std::forward<Args>(args)...);

    return shm_ptr<T>(ptr.address());
}

template<typename T>
void shm_delete_singleton(int id) {
    check_retnone(shm_has_singleton(id));

    shm_ptr<void> ptr = shm_get_singleton(id, sizeof(T), nullptr);
    assert_retnone(ptr);

    shm_ptr<T>(ptr.address())->~T();
    shm_free_singleton(id);
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
