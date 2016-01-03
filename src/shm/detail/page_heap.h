#ifndef PAGE_HEAP_H
#define PAGE_HEAP_H

#include "span.h"
#include "page_map.h"
#include "metadata_allocator.h"

namespace sk {
namespace detail {

/**
 * Heap for page level allocation.
 */
struct page_heap {
    page_map span_map;
    shm_ptr<span> free_lists[MAX_PAGES];
    shm_ptr<span> large_list;
    metadata_allocator<span> span_allocator;

    struct {
    } stat;

    void init() {
        span_map.init();

        // TODO: it's ok here, but maybe risky
        memset(free_lists, 0x00, sizeof(free_lists));
        large_list = SHM_NULL;

        span_allocator.init();
    }

    shm_ptr<span> allocate_span(int page_count);
    void deallocate_span(shm_ptr<span> ptr);

    shm_ptr<span> __allocate_large(int page_count);

    shm_ptr<span> __page2span(page_t page);

    shm_ptr<span>& __get_head(shm_ptr<span> ptr);

    void __link(shm_ptr<span> ptr);
    void __unlink(shm_ptr<span> ptr);

    shm_ptr<span> __new_span();
    void __del_span(shm_ptr<span> ptr);

    shm_ptr<span> __carve(shm_ptr<span> ptr, int page_count);

    bool __grow_heap(int page_count);
};

} // namespace detail
} // namespace sk

#endif // PAGE_HEAP_H
