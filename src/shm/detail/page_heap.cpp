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

    sk_debug("estimate space, span map: %dKB, span: %dKB.", (int)(map_size / 1024.0), (int)(span_size / 1024.0));

    return map_size + span_size;
}

void page_heap::init() {
    span_map.init();

    memset(free_lists, 0x00, sizeof(free_lists));
    memset(&large_list, 0x00, sizeof(large_list));

    // init the double linked span list
    for (int i = 0; i < MAX_PAGES; ++i) {
        offset_ptr<span> head = free_lists[i].ptr();
        span_list_init(head);
    }
    {
        offset_ptr<span> head = large_list.ptr();
        span_list_init(head);
    }

    span_allocator.init();

    memset(&stat, 0x00, sizeof(stat));
}

void page_heap::report() {
    sk_assert(stat.used_size <= stat.total_size);
    sk_assert(stat.alloc_count >= stat.free_count);

    span_allocator.report();

    sk_info("page heap => allocation count: %lu, deallocation count: %lu.",
            stat.alloc_count, stat.free_count);
    sk_info("page heap => grow count: %lu.", stat.grow_count);
    sk_info("page heap => page count, total: %lu, used: %lu (%.2lf%%).",
            stat.total_size, stat.used_size, stat.total_size <= 0 ? 0.0 : stat.used_size * 100.0 / stat.total_size);
}

offset_ptr<span> page_heap::allocate_span(int page_count) {
    assert_retval(page_count > 0, offset_ptr<span>::null());

    offset_ptr<span> ret = __search_existing(page_count);
    if (ret) {
        ++stat.alloc_count;
        stat.used_size += page_count;
        return ret;
    }

    if (__grow_heap(page_count)) {
        offset_ptr<span> ret = __search_existing(page_count);
        if (ret) {
            ++stat.alloc_count;
            stat.used_size += page_count;
        }

        return ret;
    }

    sk_error("cannot grow heap, allocation failed, page count: %d.", page_count);
    return offset_ptr<span>::null();
}

void page_heap::deallocate_span(offset_ptr<span> ptr) {
    assert_retnone(ptr);

    span *s = ptr.get();

    sk_assert(s->in_use);
    sk_assert(s->count > 0);
    sk_assert(!s->prev);
    sk_assert(!s->next);
    sk_assert(find_span(s->start) == ptr);
    sk_assert(find_span(s->start + s->count - 1) == ptr);

    s->in_use = false;

    const page_t orig_start = s->start;
    const int    orig_count = s->count;

    offset_ptr<span> prev = find_span(orig_start - 1);
    if (prev) {
        span *p = prev.get();
        if (!p->in_use) {
            sk_assert(p->count > 0);
            sk_assert(p->start + p->count == orig_start);

            s->start -= p->count;
            s->count += p->count;

            span_list_remove(prev);
            __del_span(prev);

            span_map.set(s->start, ptr.as<void>());
        }
    }

    offset_ptr<span> next = find_span(orig_start + orig_count);
    if (next) {
        span *n = next.get();
        if (!n->in_use) {
            sk_assert(n->count > 0);
            sk_assert(n->start - orig_count == orig_start);

            s->count += n->count;

            span_list_remove(next);
            __del_span(next);

            span_map.set(s->start + s->count - 1, ptr.as<void>());
        }
    }

    __link(ptr);

    ++stat.free_count;
    sk_assert(stat.used_size >= (size_t) orig_count);
    stat.used_size -= orig_count;

    // TODO: may try to return some memory to shm_mgr here
    // if the top most block is empty
}

offset_ptr<span> page_heap::find_span(page_t page) {
    return span_map.get(page).as<span>();
}

void page_heap::register_span(offset_ptr<span> ptr) {
    assert_retnone(ptr);

    span *s = ptr.get();
    sk_assert(s->in_use);
    sk_assert(find_span(s->start) == ptr);
    sk_assert(find_span(s->start + s->count - 1) == ptr);

    for (int i = 1; i < s->count - 1; ++i)
        span_map.set(s->start + i, ptr.as<void>());
}

offset_ptr<span> page_heap::__search_existing(int page_count) {
    for (int i = page_count; i < MAX_PAGES; ++i) {
        offset_ptr<span> head = free_lists[i].ptr();
        if (!span_list_empty(head))
            return __carve(head.get()->next, page_count);
    }

    return __allocate_large(page_count);
}

offset_ptr<span> page_heap::__allocate_large(int page_count) {
    offset_ptr<span> best;

    offset_ptr<span> it = large_list.next;
    offset_ptr<span> end = large_list.ptr();
    while (it != end) {
        span *s = it.get();
        if (s->count < page_count) {
            it = s->next;
            continue;
        }

        if (!best) {
            best = it;
            it = s->next;
            continue;
        }

        span *b = best.get();
        if (s->count < b->count || (s->count == b->count && s->start < b->start)) {
            best = it;
            it = s->next;
            continue;
        }

        it = s->next;
    }

    if (best)
        return __carve(best, page_count);

    return offset_ptr<span>::null();
}

offset_ptr<span> page_heap::__carve(offset_ptr<span> ptr, int page_count) {
    span *s = ptr.get();
    assert_retval(!s->in_use, offset_ptr<span>::null());
    assert_retval(s->count >= page_count, offset_ptr<span>::null());

    span_list_remove(ptr);

    const int extra = s->count - page_count;
    if (extra > 0) {
        offset_ptr<span> left = __new_span();
        span *l = left.get();
        l->init(s->start + page_count, extra);

        span_map.set(l->start, left.as<void>());
        if (l->count > 1)
            span_map.set(l->start + l->count - 1, left.as<void>());

        offset_ptr<span> next = find_span(l->start + l->count);
        sk_assert(!next || next.get()->in_use);

        __link(left);
        s->count = page_count;
        span_map.set(s->start + s->count - 1, ptr.as<void>());
    }

    s->in_use = true;
    return ptr;
}

void page_heap::__link(offset_ptr<span> ptr) {
    span *s = ptr.get();
    int page_count = s->count;
    span *head = page_count < MAX_PAGES ? &free_lists[page_count] : &large_list;

    span_list_prepend(head->ptr(), ptr);
}

offset_ptr<span> page_heap::__new_span() {
    return span_allocator.allocate();
}

void page_heap::__del_span(offset_ptr<span> ptr) {
    return span_allocator.deallocate(ptr);
}

bool page_heap::__grow_heap(int page_count) {
    assert_retval(page_count > 0, false);
    sk_assert(MAX_PAGES >= HEAP_GROW_PAGE_COUNT);

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

    sk_assert(offset % PAGE_SIZE == 0);
    const page_t p = offset >> PAGE_SHIFT;

    offset_ptr<span> sp = __new_span();
    if (!sp) {
        // TODO: handle the allocated memory here...
        sk_assert(0);
        return false;
    }

    span *s = sp.get();
    s->init(p, ask);
    span_map.set(p, sp.as<void>());
    if (ask > 1)
        span_map.set(p + ask - 1, sp.as<void>());

    s->in_use = true;

    // in fact, this is not an actual allocation here, however,
    // in deallocate_span() stat.free_count & stat.used_size will
    // be changed, to match that, we should also change the
    // stat.alloc_count & stat.used_size here
    ++stat.alloc_count;
    stat.used_size += ask;

    deallocate_span(sp);

    return true;
}

} // namespace detail
} // namespace sk
