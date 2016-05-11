#include "libsk.h"
#include "span.h"
#include "size_map.h"
#include "page_heap.h"
#include "chunk_cache.h"

namespace sk {
namespace detail {

void chunk_cache::init() {
    for (int i = 0; i < SIZE_CLASS_COUNT; ++i) {
        shm_ptr<span> head = free_lists[i].ptr();
        span_list_init(head);
    }
}

shm_ptr<void> chunk_cache::allocate(size_t bytes) {
    assert_retval(bytes <= MAX_SIZE, SHM_NULL);

    shm_mgr *mgr = shm_mgr::get();

    int sc = mgr->size_map->size_class(bytes);
    assert_retval(sc >= 0 && sc < SIZE_CLASS_COUNT, SHM_NULL);

    bytes = mgr->size_map->class_to_size(sc);

    shm_ptr<span> head = free_lists[sc].ptr();
    shm_ptr<span> sp = SHM_NULL;

    do {
        if (!span_list_empty(head)) {
            sp = head->next;
            break;
        }

        // no span in list, fetch one from page heap
        int page_count = mgr->size_map->class_to_pages(sc);
        sp = mgr->page_heap->allocate_span(page_count);

        if (!sp)
            return SHM_NULL;

        mgr->page_heap->register_span(sp);
        sp->partition(bytes, sc);
        span_list_prepend(head, sp);

    } while (0);

    span *s = sp.get();
    assert_retval(s->chunk_list, SHM_NULL);

    shm_ptr<void> ret = s->chunk_list;

    s->chunk_list = *cast_ptr(shm_ptr<void>, s->chunk_list.get());
    s->used_count += 1;

    // the span is full
    if (!s->chunk_list)
        span_list_remove(sp);

    return ret;
}

void chunk_cache::deallocate(shm_ptr<void> ptr) {
    assert_retnone(ptr);

    page_t page = shm_mgr::get()->ptr2page(ptr);
    shm_ptr<span> sp = shm_mgr::get()->page_heap->find_span(page);
    assert_retnone(sp);
    assert_retnone(sp->used_count > 0);

    span *s = sp.get();
    int sc = s->size_class;
    assert_retnone(sc >= 0 && sc < SIZE_CLASS_COUNT);

    shm_ptr<span> head = free_lists[sc].ptr();

    bool full_before = !s->chunk_list;

    *cast_ptr(shm_ptr<void>, ptr.get()) = s->chunk_list;
    s->chunk_list = ptr;
    s->used_count -= 1;

    bool empty_after = s->used_count <= 0;

    if (!full_before) {
        if (empty_after) {
            span_list_remove(sp);
            sp->erase();
            shm_mgr::get()->page_heap->deallocate_span(sp);
        } else {
            // do nothing here
        }
    } else {
        if (empty_after) {
            sp->erase();
            shm_mgr::get()->page_heap->deallocate_span(sp);
        } else {
            span_list_prepend(head, sp);
        }
    }
}

} // namespace detail
} // namespace sk
