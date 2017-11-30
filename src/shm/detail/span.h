#ifndef SPAN_H
#define SPAN_H

#include <utility/types.h>
#include <shm/detail/shm_address.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

struct span {
    u32 in_use     :  1; // the span is in use or not
    u32 used_count : 31; // how many chunks have been allocated
    int size_class;      // size class of this span

    size_t block      : shm_config::MAX_BLOCK_BITS;    // which block the memory managed by this span belongs to
    size_t start_page : shm_config::MAX_PAGE_BITS;     // the start page of this span
    size_t page_count : shm_config::MAX_PAGE_BITS + 1; // how many pages of this span, +1 here because page_count starts from 1
    size_t padding    : 9;

    shm_address prev_span;  // prev span in list
    shm_address next_span;  // next span in list
    shm_address chunk_list; // a list of chunks in the memory managed by this span

    span() : in_use(0), used_count(0), size_class(-1),
             block(0), start_page(0), page_count(0), padding(0),
             prev_span(nullptr), next_span(nullptr), chunk_list(nullptr) {}

    span(block_t block, page_t start, size_t count)
        : in_use(0), used_count(0), size_class(-1),
          block(block), start_page(start), page_count(count), padding(0),
          prev_span(nullptr), next_span(nullptr), chunk_list(nullptr) {
        // check if the numbers overflow
        sk_assert(this->block      == block);
        sk_assert(this->start_page == start);
        sk_assert(this->page_count == count);
    }

    /*
     * returns the address of this span
     */
    shm_address addr() {
        shm_address addr = shm_address::from_ptr(this);
        sk_assert(addr);
        return addr;
    }

    /*
     * partition() is used to build up linked chunk list, erase() is used to
     * destroy the list, after that, the span can be returned back to page heap
     */
    void partition(size_t bytes, u8 size_class);
    void erase();
};
static_assert(sizeof(span) == sizeof(u32) + sizeof(int) + sizeof(size_t) + sizeof(shm_address) * 3, "invalid span");

inline void span_list_init(shm_address list) {
    span *l = list.as<span>();
    l->prev_span = list;
    l->next_span = list;
}

inline bool span_list_empty(shm_address list) {
    span *l = list.as<span>();
    return l->next_span == list;
}

inline void span_list_remove(shm_address node) {
    span *n = node.as<span>();
    span *prev = n->prev_span.as<span>();
    span *next = n->next_span.as<span>();

    prev->next_span = n->next_span;
    next->prev_span = n->prev_span;
    n->prev_span = nullptr;
    n->next_span = nullptr;
}

inline void span_list_prepend(shm_address list, shm_address node) {
    span *l = list.as<span>();
    span *n = node.as<span>();
    span *next = l->next_span.as<span>();

    assert_retnone(!n->prev_span);
    assert_retnone(!n->next_span);

    n->next_span    = l->next_span;
    n->prev_span    = list;
    next->prev_span = node;
    l->next_span    = node;
}

NS_END(detail)
NS_END(sk)

#endif // SPAN_H
