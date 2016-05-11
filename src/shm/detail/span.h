#ifndef SPAN_H
#define SPAN_H

#include "shm/shm_mgr.h"
#include "shm/shm_ptr.h"
#include "utility/types.h"
#include "utility/assert_helper.h"

namespace sk {
namespace detail {

/**
 * A span represents a continuous set of page(s).
 */
struct span {
    bool in_use;  // this span is in use or not
    int count;    // page count
    page_t start; // the start page id
    shm_ptr<span> prev; // previous span in list
    shm_ptr<span> next; // next span in list
    shm_ptr<void> chunk_list; // chunks in pages managed by this span
    int used_count; // allocated chunk count
    int size_class; // the size class

    inline shm_ptr<span> ptr() {
        offset_t offset = shm_mgr::get()->ptr2offset(this);
        return shm_ptr<span>(offset);
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

inline void span_list_init(shm_ptr<span> list) {
    list->prev = list;
    list->next = list;
}

inline bool span_list_empty(shm_ptr<span> list) {
    return list->next == list;
}

inline void span_list_remove(shm_ptr<span> node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = SHM_NULL;
    node->next = SHM_NULL;
}

inline void span_list_prepend(shm_ptr<span> list, shm_ptr<span> node) {
    assert_retnone(!node->prev);
    assert_retnone(!node->next);

    node->next = list->next;
    node->prev = list;
    list->next->prev = node;
    list->next = node;
}

} // namespace detail
} // namespace sk

#endif // SPAN_H
