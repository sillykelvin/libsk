#ifndef CHUNK_CACHE_H
#define CHUNK_CACHE_H

#include "shm/detail/span.h"
#include "utility/config.h"

namespace sk {
namespace detail {

struct class_cache {
    span free_list;
    int span_count;

    void init();
};

struct chunk_cache {
    class_cache caches[SIZE_CLASS_COUNT];

    struct {
        size_t span_alloc_count; // how many span has allocated from heap
        size_t span_free_count;  // how many span has returned to heap
        size_t alloc_count; // how many allocation has happened
        size_t free_count;  // how many deallocation has happened
    } stat;

    void init();

    void report();

    shm_ptr<void> allocate(size_t bytes);
    void deallocate(shm_ptr<void> ptr);
};

} // namespace detail
} // namespace sk

#endif // CHUNK_CACHE_H
