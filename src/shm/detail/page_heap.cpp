#include "page_heap.h"

namespace sk {
namespace detail {

shm_ptr<span> page_heap::allocate_span(int page_count) {
    assert_retval(page_count > 0, SHM_NULL);

    for (int i = page_count; i < MAX_PAGES; ++i) {
        if (free_lists[i])
            return __carve(free_lists[i], page_count);
    }

    return __allocate_large(page_count);
}

void page_heap::deallocate_span(shm_ptr<span> ptr) {
    if (!ptr) {
        assert_noeffect(0);
        return;
    }

    span *s = ptr.get();

    assert_noeffect(s->in_use);
    assert_noeffect(s->count > 0);
    assert_noeffect(!s->prev);
    assert_noeffect(!s->next);
    assert_noeffect(__page2span(s->start) == ptr);
    assert_noeffect(__page2span(s->start + s->count - 1) == ptr);

    s->in_use = false;

    const page_t orig_start = s->start;
    const int    orig_count = s->count;

    shm_ptr<span> prev = __page2span(orig_start - 1);
    if (prev && !prev->in_use) {
        span *p = prev.get();
        assert_noeffect(p->count > 0);
        assert_noeffect(p->start + p->count == orig_start);

        s->start -= p->count;
        s->count += p->count;

        __unlink(prev);
        __del_span(prev);

        span_map.set(s->start, ptr);
    }

    shm_ptr<span> next = __page2span(orig_start + orig_count);
    if (next && !next->in_use) {
        span *n = next.get();
        assert_noeffect(n->count > 0);
        assert_noeffect(n->start - orig_count == orig_start);

        s->count += n->count;

        __unlink(next);
        __del_span(next);

        span_map.set(s->start + s->count - 1, ptr);
    }

    __link(ptr);
}

shm_ptr<span> page_heap::__allocate_large(int page_count) {
    shm_ptr<span> best;

    if (!large_list)
        return SHM_NULL;

    shm_ptr<span> ptr = large_list;
    do {
        span *s = ptr.get();
        if (s->count >= page_count) {
            if (!best || s->count < best->count || (s->count == best->count && s->start < best->start))
                best = ptr;
        }

        ptr = s->next;
    } while (ptr != large_list);

    if (best)
        return __carve(best, page_count);

    return SHM_NULL;
}

shm_ptr<span> page_heap::__page2span(page_t page) {
    return span_map.get(page);
}

shm_ptr<span>& page_heap::__get_head(shm_ptr<span> ptr) {
    int count = ptr->count;
    if (count < MAX_PAGES)
        return free_lists[count];

    return large_list;
}

void page_heap::__link(shm_ptr<span> ptr) {
    span *s = ptr.get();
    shm_ptr<span>& head = __get_head(ptr);

    if (!head) {
        head = ptr;
        s->prev = ptr;
        s->next = ptr;

        return;
    }

    span *h = head.get();
    s->next = head;
    s->prev = h->prev;
    h->prev->next = ptr;
    h->prev = ptr;
    head = ptr;
}

void page_heap::__unlink(shm_ptr<span> ptr) {
    span *s = ptr.get();
    shm_ptr<span>& head = __get_head(ptr);

    if (head == ptr)
        head = s->next;

    s->prev->next = s->next;
    s->next->prev = s->prev;
    s->prev = SHM_NULL;
    s->next = SHM_NULL;
}

shm_ptr<span> page_heap::__new_span() {
    return span_allocator.allocate();
}

void page_heap::__del_span(shm_ptr<span> ptr) {
    return span_allocator.deallocate(ptr);
}

shm_ptr<span> page_heap::__carve(shm_ptr<span> ptr, int page_count) {
    span *s = ptr.get();
    assert_noeffect(s->count >= page_count);
    assert_noeffect(!s->in_use);

    __unlink(ptr);

    const int extra = s->count - page_count;
    if (extra > 0) {
        shm_ptr<span> left = __new_span();
        span *l = left.get();
        l->init(s->start + page_count, extra);

        span_map.set(l->start, left);
        if (l->count > 1)
            span_map.set(l->start + l->count - 1, left);

        shm_ptr<span> next = __page2span(l->start + l->count);
        assert_noeffect(!next || next->in_use);

        __link(left);
        s->count = page_count;
        span_map.set(s->start + s->count - 1, ptr);
    }

    s->in_use = true;
    return ptr;
}

bool page_heap::__grow_heap(int page_count) {
    // TODO: implement this function
    (void) page_count;
    return false;
}
} // namespace detail
} // namespace sk
