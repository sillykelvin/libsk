#ifndef PAGE_HEAP_H
#define PAGE_HEAP_H

#include <shm/detail/span.h>
#include <shm/detail/radix_tree.h>
#include <shm/detail/metadata_allocator.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

class page_heap {
public:
    page_heap();

    shm_address allocate_span(size_t page_count);
    void deallocate_span(shm_address sp);
    void register_span(shm_address sp);
    shm_address find_span(shm_address addr);

private:
    shm_address search_existing(size_t page_count);
    shm_address allocate_large(size_t page_count);

    shm_address carve(shm_address sp, size_t page_count);
    void link(shm_address sp);

    shm_address new_span(block_t block, page_t start_page, size_t page_count);
    void del_span(shm_address sp);

    // TODO: the page might overflow as we limited it to MAX_PAGE_BITS
    // be careful to check this
    shm_address get_span_map(block_t block, page_t page) const;
    void set_span_map(block_t block, page_t page, shm_address sp);

    bool grow_heap(size_t page_count);

private:
    static const size_t LV2_BITS = shm_config::MAX_PAGE_BITS / 2;
    static const size_t LV1_BITS = shm_config::MAX_PAGE_BITS - LV2_BITS;
    static const size_t LV0_BITS = shm_config::MAX_BLOCK_BITS;

    radix_tree<shm_address, LV0_BITS, LV1_BITS, LV2_BITS> span_map_;
    metadata_allocator<span> span_allocator_;
    span free_lists_[shm_config::MAX_PAGES];
    span large_list_;

    struct stat {
        stat() { memset(this, 0x00, sizeof(*this)); }

        size_t used_size;   // how many pages has been used currently
        size_t total_size;  // total pages managed by page heap
        size_t grow_count;  // how many times has heap grown
        size_t alloc_count; // how many allocation has happened
        size_t free_count;  // how many deallocation has happened
    } stat_;
};

NS_END(detail)
NS_END(sk)

#endif // PAGE_HEAP_H
