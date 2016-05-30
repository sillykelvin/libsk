#include "libsk.h"
#include "span.h"
#include "page_map.h"
#include "metadata_allocator.h"
#include "page_heap.h"

namespace sk {
namespace detail {

size_t page_heap::estimate_space(size_t bytes) {
    int page_count = (bytes >> PAGE_SHIFT) + ((bytes & (PAGE_SIZE - 1)) > 0 ? 1 : 0);

    // shm_mgr and other structs will consume some pages at the
    // beginning of the pool, so add a reasonable large number
    // here to ensure the page count is enough
    page_count += 8;

    size_t map_size = page_map::estimate_space(page_count);

    // the limiting case is every page is mapped to a span
    size_t span_size = page_count * sizeof(span);
    // metadata allocator will allocate META_ALLOC_INCREMENT bytes at a time
    if (span_size < META_ALLOC_INCREMENT)
        span_size = META_ALLOC_INCREMENT;

    DBG("estimate space, span map: %dKB, span: %dKB.", (int)(map_size / 1024.0), (int)(span_size / 1024.0));

    return map_size + span_size;
}

void page_heap::init() {
    span_map.init();

    memset(free_lists, 0x00, sizeof(free_lists));
    memset(&large_list, 0x00, sizeof(large_list));

    // init the double linked span list
    for (int i = 0; i < MAX_PAGES; ++i) {
        shm_ptr<span> head = free_lists[i].ptr();
        span_list_init(head);
    }
    {
        shm_ptr<span> head = large_list.ptr();
        span_list_init(head);
    }

    span_allocator.init();

    memset(&stat, 0x00, sizeof(stat));
}

void page_heap::report() {
    assert_noeffect(stat.alloc_size >= stat.free_size);
    assert_noeffect(stat.alloc_count >= stat.free_count);

    span_allocator.report();

    INF("page heap => page count, total: %lu, allocated: %lu, deallocated: %lu.",
        stat.total_size, stat.alloc_size, stat.free_size);
    INF("page heap => allocation count: %lu, deallocation count: %lu.",
        stat.alloc_count, stat.free_count);
    INF("page heap => grow count: %lu.", stat.grow_count);

    size_t used_size = stat.alloc_size - stat.free_size;
    INF("page heap => used page count: %lu, size: %lu, percentage: (%.2lf%%).",
        used_size, used_size << PAGE_SHIFT, used_size * 100.0 / stat.total_size);
}

shm_ptr<span> page_heap::allocate_span(int page_count) {
    assert_retval(page_count > 0, SHM_NULL);

    shm_ptr<span> ret = __search_existing(page_count);
    if (ret) {
        ++stat.alloc_count;
        stat.alloc_size += page_count;
        return ret;
    }

    if (__grow_heap(page_count)) {
        shm_ptr<span> ret = __search_existing(page_count);
        if (ret) {
            ++stat.alloc_count;
            stat.alloc_size += page_count;
        }

        return ret;
    }

    ERR("cannot grow heap, allocation failed, page count: %d.", page_count);
    return SHM_NULL;
}

void page_heap::deallocate_span(shm_ptr<span> ptr) {
    assert_retnone(ptr);

    span *s = ptr.get();

    assert_noeffect(s->in_use);
    assert_noeffect(s->count > 0);
    assert_noeffect(!s->prev);
    assert_noeffect(!s->next);
    assert_noeffect(find_span(s->start) == ptr);
    assert_noeffect(find_span(s->start + s->count - 1) == ptr);

    s->in_use = false;

    const page_t orig_start = s->start;
    const int    orig_count = s->count;

    shm_ptr<span> prev = find_span(orig_start - 1);
    if (prev && !prev->in_use) {
        span *p = prev.get();
        assert_noeffect(p->count > 0);
        assert_noeffect(p->start + p->count == orig_start);

        s->start -= p->count;
        s->count += p->count;

        span_list_remove(prev);
        __del_span(prev);

        span_map.set(s->start, ptr);
    }

    shm_ptr<span> next = find_span(orig_start + orig_count);
    if (next && !next->in_use) {
        span *n = next.get();
        assert_noeffect(n->count > 0);
        assert_noeffect(n->start - orig_count == orig_start);

        s->count += n->count;

        span_list_remove(next);
        __del_span(next);

        span_map.set(s->start + s->count - 1, ptr);
    }

    __link(ptr);

    ++stat.free_count;
    stat.free_size += orig_count;

    // TODO: may try to return some memory to shm_mgr here
    // if the top most block is empty
}

shm_ptr<span> page_heap::find_span(page_t page) {
    return span_map.get(page);
}

void page_heap::register_span(shm_ptr<span> ptr) {
    assert_retnone(ptr);

    span *s = ptr.get();
    assert_noeffect(s->in_use);
    assert_noeffect(find_span(s->start) == ptr);
    assert_noeffect(find_span(s->start + s->count - 1) == ptr);

    for (int i = 1; i < s->count - 1; ++i)
        span_map.set(s->start + i, ptr);
}

shm_ptr<span> page_heap::__search_existing(int page_count) {
    for (int i = page_count; i < MAX_PAGES; ++i) {
        shm_ptr<span> head = free_lists[i].ptr();
        if (!span_list_empty(head))
            return __carve(head->next, page_count);
    }

    return __allocate_large(page_count);
}

shm_ptr<span> page_heap::__allocate_large(int page_count) {
    shm_ptr<span> best;

    shm_ptr<span> it, end;
    for (it = large_list.next, end = large_list.ptr(); it != end; it = it->next) {
        span *s = it.get();
        if (s->count < page_count)
            continue;

        if (!best
            || s->count < best->count
            || (s->count == best->count && s->start < best->start))
            best = it;
    }

    if (best)
        return __carve(best, page_count);

    return SHM_NULL;
}

shm_ptr<span> page_heap::__carve(shm_ptr<span> ptr, int page_count) {
    span *s = ptr.get();
    assert_retval(!s->in_use, SHM_NULL);
    assert_retval(s->count >= page_count, SHM_NULL);

    span_list_remove(ptr);

    const int extra = s->count - page_count;
    if (extra > 0) {
        shm_ptr<span> left = __new_span();
        span *l = left.get();
        l->init(s->start + page_count, extra);

        span_map.set(l->start, left);
        if (l->count > 1)
            span_map.set(l->start + l->count - 1, left);

        shm_ptr<span> next = find_span(l->start + l->count);
        assert_noeffect(!next || next->in_use);

        __link(left);
        s->count = page_count;
        span_map.set(s->start + s->count - 1, ptr);
    }

    s->in_use = true;
    return ptr;
}

void page_heap::__link(shm_ptr<span> ptr) {
    int page_count = ptr->count;
    span *head = page_count < MAX_PAGES ? &free_lists[page_count] : &large_list;

    span_list_prepend(head->ptr(), ptr);
}

shm_ptr<span> page_heap::__new_span() {
    return span_allocator.allocate();
}

void page_heap::__del_span(shm_ptr<span> ptr) {
    return span_allocator.deallocate(ptr);
}

bool page_heap::__grow_heap(int page_count) {
    assert_retval(page_count > 0, false);
    assert_noeffect(MAX_PAGES >= HEAP_GROW_PAGE_COUNT);

    if (page_count > MAX_VALID_PAGES)
        return false;

    int ask = page_count;
    if (page_count < HEAP_GROW_PAGE_COUNT)
        ask = HEAP_GROW_PAGE_COUNT;

    offset_t offset = shm_mgr::get()->allocate(ask << PAGE_SHIFT);
    do {
        if (offset != OFFSET_NULL)
            break;

        if (page_count >= ask)
            return false;

        ask = page_count;
        offset = shm_mgr::get()->allocate(ask << PAGE_SHIFT);
        if (offset != OFFSET_NULL)
            break;

        return false;
    } while (0);

    stat.grow_count += 1;
    stat.total_size += ask;

    assert_noeffect(offset % PAGE_SIZE == 0);
    const page_t p = offset >> PAGE_SHIFT;

    shm_ptr<span> s = __new_span();
    if (!s) {
        // TODO: handle the allocated memory here...
        assert_noeffect(0);
        return false;
    }

    s->init(p, ask);
    span_map.set(p, s);
    if (ask > 1)
        span_map.set(p + ask - 1, s);

    s->in_use = true;

    // in fact, this is not an actual allocation here, however,
    // in deallocate_span() stat.free_count & stat.free_size will
    // be increased, to match that, we should also increase the
    // stat.alloc_count & stat.alloc_size here
    ++stat.alloc_count;
    stat.alloc_size += ask;

    deallocate_span(s);

    return true;
}

} // namespace detail
} // namespace sk
