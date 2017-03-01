#ifndef SPAN_H
#define SPAN_H

#include "shm/shm_mgr.h"
#include "utility/types.h"
#include "utility/assert_helper.h"
#include "shm/detail/offset_ptr.h"

namespace sk {
namespace detail {

/**
 * A span represents a continuous set of page(s).
 */
struct span {
    bool in_use;                 // this span is in use or not
    int count;                   // page count
    page_t start;                // the start page id
    int used_count;              // allocated chunk count
    int size_class;              // the size class
    offset_ptr<span> prev;       // previous span in list
    offset_ptr<span> next;       // next span in list
    offset_ptr<void> chunk_list; // chunks in pages managed by this span

    inline offset_ptr<span> ptr() {
        offset_t offset = shm_mgr::get()->ptr2offset(this);
        return offset_ptr<span>(offset);
    }

    void init(page_t start, int count);

    /*
     * partition() is used to build up linked chunk list,
     * erase() will destroy the info, and then, the span
     * can be returned back to page heap
     */
    void partition(size_t bytes, int size_class);
    void erase();
};

inline void span_list_init(offset_ptr<span> list) {
    span *l = list.get();
    l->prev = list;
    l->next = list;
}

inline bool span_list_empty(offset_ptr<span> list) {
    span *l = list.get();
    return l->next == list;
}

inline void span_list_remove(offset_ptr<span> node) {
    span *n = node.get();
    span *prev = n->prev.get();
    span *next = n->next.get();

    prev->next = n->next;
    next->prev = n->prev;
    n->prev.set_null();
    n->next.set_null();
}

inline void span_list_prepend(offset_ptr<span> list, offset_ptr<span> node) {
    span *l = list.get();
    span *n = node.get();
    span *next = l->next.get();

    assert_retnone(!n->prev);
    assert_retnone(!n->next);

    n->next = l->next;
    n->prev = list;
    next->prev = node;
    l->next = node;
}

} // namespace detail
} // namespace sk

#endif // SPAN_H
