#ifndef SHM_MGR_H
#define SHM_MGR_H

#include <shm/shm_ptr.h>
#include <shm/detail/shm_address.h>

NS_BEGIN(sk)

NS_BEGIN(detail)
class size_map;
class page_heap;
class block_mgr;
class chunk_cache;
NS_END(detail)

struct shm_option {
    bool enable_addr2block_mapping;
    bool zero_memory_after_malloc;

    shm_option()
        : enable_addr2block_mapping(false),
          zero_memory_after_malloc(false) {}
};

/*
 * the reserved shm singleton types
 */
enum shm_singleton_type {
    SHM_SINGLETON_SHM_TIMER_MGR = 0,
    SHM_SINGLETON_RESERVED_MAX
};

class shm_mgr {
public:
    static int init(const char *basename, bool resume_mode);

    static shm_ptr<void> malloc(size_t bytes);
    static void free(const shm_ptr<void>& ptr);

    static bool has_singleton(int type);
    static shm_ptr<void> get_singleton(int type, size_t bytes, bool *first_call);
    static void free_singleton(int type);

    static void *addr2ptr(detail::shm_address addr);
    static detail::shm_address ptr2addr(void *ptr);

public:
    static detail::size_map    *size_map();
    static detail::block_mgr   *block_mgr();
    static detail::page_heap   *page_heap();
    static detail::chunk_cache *chunk_cache();

    static detail::shm_address allocate(size_t in_bytes, size_t *out_bytes);
    static detail::shm_address allocate_metadata(size_t bytes);

private:
    shm_mgr()
        : serial_(0), special_block_(cast_size(-1)),
          size_map_(nullptr), block_mgr_(nullptr),
          page_heap_(nullptr), chunk_cache_(nullptr) {
        memset(&metadata_, 0x00, sizeof(metadata_));
        memset(&stat_, 0x00, sizeof(stat_));
    }

    int on_create(const char *basename);
    int on_resume(const char *basename);

private:
    static const int MAX_SINGLETON_COUNT = 256;

    size_t serial_;
    detail::block_t special_block_;
    shm_ptr<void> singletons_[MAX_SINGLETON_COUNT];

    struct {
        detail::block_t current_meta_block;
        detail::offset_t current_meta_left;
        detail::offset_t current_meta_offset;
    } metadata_;

    struct {
        // TODO: fill & reset the waste_bytes field at proper place
        size_t waste_bytes;      // wasted bytes due to size alignment
        size_t alloc_count;      // total allocation count
        size_t free_count;       // total deallocation count
        size_t raw_alloc_count;  // raw memory allocation count
        size_t meta_alloc_count; // metadata allocation count
    } stat_;

    detail::size_map    *size_map_;
    detail::block_mgr   *block_mgr_;
    detail::page_heap   *page_heap_;
    detail::chunk_cache *chunk_cache_;
};

shm_ptr<void> shm_malloc(size_t bytes);
void shm_free(const shm_ptr<void>& ptr);

template<typename T, typename... Args>
shm_ptr<T> shm_new(Args&&... args) {
    shm_ptr<T> ptr(shm_malloc(sizeof(T)));
    if (!ptr) return nullptr;

    T *t = ptr.get();
    new (t) T(std::forward<Args>(args)...);

    return ptr;
}

template<typename T>
void shm_delete(const shm_ptr<T>& ptr) {
    if (!ptr) return;

    const T *t = ptr.get();
    t->~T();

    shm_free(shm_ptr<void>(ptr));
}

template<typename T, typename... Args>
shm_ptr<T> shm_get_singleton(int type, Args&&... args) {
    bool first_call = false;
    shm_ptr<T> ptr(shm_mgr::get_singleton(type, sizeof(T), &first_call));
    if (!ptr) return nullptr;

    if (first_call) {
        T *t = ptr.get();
        new (t) T(std::forward<Args>(args)...);
    }

    return ptr;
};

template<typename T>
void shm_delete_singleton(int type) {
    if (!shm_mgr::has_singleton(type)) return;

    shm_ptr<T> ptr(shm_mgr::get_singleton(type, sizeof(T), nullptr));
    ptr->~T();

    shm_mgr::free_singleton(type);
}

int shm_mgr_init(const char *basename, bool resume_mode);
int shm_mgr_fini();

NS_END(sk)

#endif // SHM_MGR_H
