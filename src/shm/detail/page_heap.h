#ifndef PAGE_HEAP_H
#define PAGE_HEAP_H

#include "shm/shm_ptr.h"
#include "shm/detail/span.h"
#include "shm/detail/page_map.h"
#include "shm/detail/metadata_allocator.h"
#include "utility/config.h"

namespace sk {
namespace detail {

/**
 * Heap for page level allocation.
 */
struct page_heap {
    page_map span_map;
    span free_lists[MAX_PAGES];
    span large_list;
    metadata_allocator<span> span_allocator;

    struct {
        size_t total_count; // how many pages has been allocated
        size_t grow_count;  // how many times has heap grown
    } stat;

    /*
     * estimate the metadata space needed by given size "bytes"
     */
    static size_t estimate_space(size_t bytes);

    void init();

    shm_ptr<span> allocate_span(int page_count);
    void deallocate_span(shm_ptr<span> ptr);
    shm_ptr<span> find_span(page_t page);
    void register_span(shm_ptr<span> ptr);

    shm_ptr<span> __search_existing(int page_count);
    shm_ptr<span> __allocate_large(int page_count);

    shm_ptr<span> __carve(shm_ptr<span> ptr, int page_count);
    void __link(shm_ptr<span> ptr);

    shm_ptr<span> __new_span();
    void __del_span(shm_ptr<span> ptr);

    bool __grow_heap(int page_count);
};

} // namespace detail
} // namespace sk

#endif // PAGE_HEAP_H
