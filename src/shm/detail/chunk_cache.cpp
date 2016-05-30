#include "libsk.h"
#include "span.h"
#include "size_map.h"
#include "page_heap.h"
#include "chunk_cache.h"

namespace sk {
namespace detail {

void class_cache::init() {
    shm_ptr<span> head = free_list.ptr();
    span_list_init(head);

    span_count = 0;

    memset(&stat, 0x00, sizeof(stat));
}

void chunk_cache::init() {
    for (int i = 0; i < SIZE_CLASS_COUNT; ++i)
        caches[i].init();

    memset(&stat, 0x00, sizeof(stat));
}

void chunk_cache::report() {
    assert_noeffect(stat.used_size <= stat.total_size);

    for (int i = 0; i < SIZE_CLASS_COUNT; ++i) {
        const class_cache& cache = caches[i];
        if (cache.stat.total_size <= 0)
            continue;

        assert_noeffect(cache.stat.used_size <= cache.stat.total_size);

        INF("chunk cache => cache[%d], bytes[%lu], span count: %d, allocation count: %lu, deallocation count: %lu, total bytes: %lu, used bytes: %lu (%.2lf%%).",
            i, shm_mgr::get()->size_map->class_to_size(i), cache.span_count,
            cache.stat.alloc_count, cache.stat.free_count,
            cache.stat.total_size, cache.stat.used_size,
            cache.stat.used_size * 100.0 / cache.stat.total_size);
    }

    INF("chunk cache => span allocation count: %lu, span deallocation count: %lu.",
        stat.span_alloc_count, stat.span_free_count);
    INF("chunk cache => allocation count: %lu, deallocation count: %lu.",
        stat.alloc_count, stat.free_count);
    INF("chunk cache => total bytes: %lu, used bytes: %lu (%.2lf%%).",
        stat.total_size, stat.used_size, stat.total_size <= 0 ? 0.0 : stat.used_size * 100.0 / stat.total_size);
}

shm_ptr<void> chunk_cache::allocate(size_t bytes) {
    assert_retval(bytes <= MAX_SIZE, SHM_NULL);

    shm_mgr *mgr = shm_mgr::get();

    int sc = mgr->size_map->size_class(bytes);
    assert_retval(sc >= 0 && sc < SIZE_CLASS_COUNT, SHM_NULL);

    bytes = mgr->size_map->class_to_size(sc);

    shm_ptr<span> head = caches[sc].free_list.ptr();
    shm_ptr<span> sp = SHM_NULL;

    do {
        if (!span_list_empty(head)) {
            assert_noeffect(caches[sc].span_count > 0);
            sp = head->next;
            break;
        }

        // no span in list, fetch one from page heap
        int page_count = mgr->size_map->class_to_pages(sc);
        sp = mgr->page_heap->allocate_span(page_count);

        if (!sp)
            return SHM_NULL;

        ++stat.span_alloc_count;
        stat.total_size += page_count << PAGE_SHIFT;
        mgr->page_heap->register_span(sp);
        sp->partition(bytes, sc);
        span_list_prepend(head, sp);
        ++caches[sc].span_count;
        caches[sc].stat.total_size += page_count << PAGE_SHIFT;

    } while (0);

    span *s = sp.get();
    assert_retval(s->chunk_list, SHM_NULL);

    shm_ptr<void> ret = s->chunk_list;

    s->chunk_list = *cast_ptr(shm_ptr<void>, s->chunk_list.get());
    s->used_count += 1;

    // the span is full
    if (!s->chunk_list) {
        span_list_remove(sp);
        --caches[sc].span_count;
        // do NOT change any stat memeber of caches[sc] here
    }

    ++caches[sc].stat.alloc_count;
    ++stat.alloc_count;
    caches[sc].stat.used_size += bytes;
    stat.used_size += bytes;

    return ret;
}

void chunk_cache::deallocate(shm_ptr<void> ptr) {
    assert_retnone(ptr);

    shm_mgr *mgr = shm_mgr::get();

    page_t page = mgr->ptr2page(ptr);
    shm_ptr<span> sp = mgr->page_heap->find_span(page);
    assert_retnone(sp);

    span *s = sp.get();
    assert_retnone(s->used_count > 0);

    const int sc = s->size_class;
    assert_retnone(sc >= 0 && sc < SIZE_CLASS_COUNT);

    shm_ptr<span> head = caches[sc].free_list.ptr();

    bool full_before = !s->chunk_list;

    *cast_ptr(shm_ptr<void>, ptr.get()) = s->chunk_list;
    s->chunk_list = ptr;
    s->used_count -= 1;

    bool empty_after = s->used_count <= 0;

    if (!full_before) {
        if (empty_after) {
            // this span itself is still in the list, so we
            // compare with 1 here
            if (caches[sc].span_count > 1) {
                span_list_remove(sp);
                --caches[sc].span_count;

                const size_t bytes = s->count << PAGE_SHIFT;
                assert_noeffect(caches[sc].stat.total_size >= bytes);
                assert_noeffect(stat.total_size >= bytes);
                caches[sc].stat.total_size -= bytes;
                stat.total_size -= bytes;
                ++stat.span_free_count;

                s->erase();
                mgr->page_heap->deallocate_span(sp);
            } else {
                // do nothing here
            }
        } else {
            // do nothing here
        }
    } else {
        if (empty_after) {
            if (caches[sc].span_count > 0) {
                const size_t bytes = s->count << PAGE_SHIFT;
                assert_noeffect(caches[sc].stat.total_size >= bytes);
                assert_noeffect(stat.total_size >= bytes);
                caches[sc].stat.total_size -= bytes;
                stat.total_size -= bytes;
                ++stat.span_free_count;

                s->erase();
                mgr->page_heap->deallocate_span(sp);
            } else {
                span_list_prepend(head, sp);
                ++caches[sc].span_count;
            }
        } else {
            span_list_prepend(head, sp);
            ++caches[sc].span_count;
        }
    }

    const size_t bytes = mgr->size_map->class_to_size(sc);
    assert_noeffect(caches[sc].stat.used_size >= bytes);
    assert_noeffect(stat.used_size >= bytes);
    caches[sc].stat.used_size -= bytes;
    stat.used_size -= bytes;
    ++caches[sc].stat.free_count;
    ++stat.free_count;
}

} // namespace detail
} // namespace sk
