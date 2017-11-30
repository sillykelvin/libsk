#ifndef CHUNK_CACHE_H
#define CHUNK_CACHE_H

#include <shm/detail/span.h>
#include <shm/detail/size_map.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

class chunk_cache {
public:
    shm_address allocate_chunk(size_t bytes, u8 sc);
    void deallocate_chunk(shm_address addr, shm_address sp);

private:
    struct class_cache {
        span free_list;
        int span_count;

        struct {
            size_t alloc_count; // how many allocations has happened
            size_t free_count;  // how many deallocations has happened
            size_t used_size;   // how many bytes has been used currently
            size_t total_size;  // total bytes managed by this class cache
        } stat;

        class_cache() {
            shm_address head = free_list.addr();
            span_list_init(head);
            span_count = 0;
            memset(&stat, 0x00, sizeof(stat));
        }
    } caches_[size_map::SIZE_CLASS_COUNT];

    struct stat {
        stat() { memset(this, 0x00, sizeof(*this)); }

        size_t span_alloc_count; // how many spans has allocated from heap
        size_t span_free_count;  // how many spans has returned to heap
        size_t alloc_count;      // how many allocations has happened
        size_t free_count;       // how many deallocations has happened
        size_t used_size;        // how many bytes has been used currently
        size_t total_size;       // total bytes managed by chunk cache
    } stat_;
};

NS_END(detail)
NS_END(sk)

#endif // CHUNK_CACHE_H
