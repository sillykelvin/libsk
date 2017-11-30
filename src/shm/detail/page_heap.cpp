#include <shm/shm_mgr.h>
#include <shm/detail/page_heap.h>

using namespace sk::detail;

page_heap::page_heap() {
    for (size_t i = 0; i < shm_config::MAX_PAGES; ++i) {
        shm_address head = free_lists_[i].addr();
        span_list_init(head);
    }

    shm_address head = large_list_.addr();
    span_list_init(head);
}

shm_address page_heap::allocate_span(size_t page_count) {
    assert_retval(page_count > 0, nullptr);

    shm_address addr = search_existing(page_count);
    if (!addr && grow_heap(page_count))
        addr = search_existing(page_count);

    if (addr) {
        ++stat_.alloc_count;
        stat_.used_size += page_count;
        return addr;
    }

    sk_error("cannot allocate span, page count: %lu.", page_count);
    return nullptr;
}

void page_heap::deallocate_span(shm_address sp) {
    assert_retnone(sp);

    span *s = sp.as<span>();
    sk_assert(s->in_use);
    sk_assert(s->page_count > 0);
    sk_assert(!s->prev_span);
    sk_assert(!s->next_span);
    sk_assert(get_span_map(s->block, s->start_page) == sp);
    sk_assert(get_span_map(s->block, s->start_page + s->page_count - 1) == sp);

    s->in_use = 0;

    const page_t orig_start = s->start_page;
    const size_t orig_count = s->page_count;

    shm_address prev = (orig_start == 0) ? nullptr : get_span_map(s->block, orig_start - 1);
    if (prev) {
        span *p = prev.as<span>();
        if (!p->in_use) {
            sk_assert(p->page_count > 0);
            sk_assert(cast_size(p->start_page + p->page_count) == orig_start);

            s->start_page -= p->page_count;
            s->page_count += p->page_count;

            span_list_remove(prev);
            del_span(prev);
            set_span_map(s->block, s->start_page, sp);
        }
    }

    // TODO: orig_start + orig_count may overflow MAX_PAGE_BITS, the same below
    shm_address next = get_span_map(s->block, orig_start + orig_count);
    if (next) {
        span *n = next.as<span>();
        if (!n->in_use) {
            sk_assert(n->page_count > 0);
            sk_assert(n->start_page - orig_count == orig_start);

            s->page_count += n->page_count;

            span_list_remove(next);
            del_span(next);
            set_span_map(s->block, s->start_page + s->page_count - 1, sp);
        }
    }

    link(sp);

    ++stat_.free_count;
    sk_assert(stat_.used_size >= orig_count);
    stat_.used_size -= orig_count;

    // TODO: may try to return some memory to shm_mgr here
    // if the top most block is empty
}

void page_heap::register_span(shm_address sp) {
    assert_retnone(sp);

    span *s = sp.as<span>();
    sk_assert(s->in_use);
    sk_assert(get_span_map(s->block, s->start_page) == sp);
    sk_assert(get_span_map(s->block, s->start_page + s->page_count - 1) == sp);

    for (size_t i = 1; i < s->page_count - cast_size(1); ++i)
        set_span_map(s->block, s->start_page + i, sp);
}

shm_address page_heap::find_span(shm_address addr) {
    return get_span_map(addr.block(), addr.offset() >> shm_config::PAGE_SHIFT);
}

shm_address page_heap::search_existing(size_t page_count) {
    for (size_t i = page_count; i < shm_config::MAX_PAGES; ++i) {
        shm_address head = free_lists_[i].addr();
        if (!span_list_empty(head))
            return carve(head.as<span>()->next_span, page_count);
    }

    return allocate_large(page_count);
}

shm_address page_heap::allocate_large(size_t page_count) {
    shm_address best(nullptr);

    shm_address it  = large_list_.next_span;
    shm_address end = large_list_.addr();
    while (it != end) {
        span *s = it.as<span>();
        if (s->page_count < page_count) {
            it = s->next_span;
            continue;
        }

        if (!best) {
            best = it;
            it = s->next_span;
            continue;
        }

        span *b = best.as<span>();
        if (s->page_count < b->page_count ||
            (s->page_count == b->page_count && s->block < b->block) ||
            (s->page_count == b->page_count && s->block == b->block && s->start_page < b->start_page)) {
            best = it;
            it = s->next_span;
            continue;
        }

        it = s->next_span;
    }

    if (best) return carve(best, page_count);

    return nullptr;
}

shm_address page_heap::carve(shm_address sp, size_t page_count) {
    span *s = sp.as<span>();
    assert_retval(!s->in_use, nullptr);
    assert_retval(s->page_count >= page_count, nullptr);

    span_list_remove(sp);

    const size_t extra = s->page_count - page_count;
    if (extra > 0) {
        // TODO: what if the allocation failed??
        shm_address left = new_span(s->block, s->start_page + page_count, extra);
        span *l = left.as<span>();

        set_span_map(l->block, l->start_page, left);
        if (l->page_count > 1)
            set_span_map(l->block, l->start_page + l->page_count - 1, left);

        shm_address next = get_span_map(l->block, l->start_page + l->page_count);
        sk_assert(!next || next.as<span>()->in_use);

        link(left);
        s->page_count = page_count;
        set_span_map(s->block, s->start_page + s->page_count - 1, sp);
    }

    s->in_use = 1;
    return sp;
}

void page_heap::link(shm_address sp) {
    span *s = sp.as<span>();
    size_t page_count = s->page_count;
    span *head = (page_count < shm_config::MAX_PAGES) ? &free_lists_[page_count] : &large_list_;

    span_list_prepend(head->addr(), sp);
}

shm_address page_heap::new_span(block_t block, page_t start_page, size_t page_count) {
    shm_address sp = span_allocator_.allocate();
    if (sp) {
        span *s = sp.as<span>();
        new (s) span(block, start_page, page_count);
    }

    return sp;
}

void page_heap::del_span(shm_address sp) {
    span_allocator_.deallocate(sp);
}

shm_address page_heap::get_span_map(block_t block, page_t page) const {
    assert_retval((block >> shm_config::MAX_BLOCK_BITS) == 0, nullptr);
    assert_retval((page >> shm_config::MAX_PAGE_BITS) == 0, nullptr);

    size_t key = (block << shm_config::MAX_PAGE_BITS) + page;
    const shm_address *addr = span_map_.get(key);
    return addr ? *addr : nullptr;
}

void page_heap::set_span_map(block_t block, page_t page, shm_address sp) {
    assert_retnone((block >> shm_config::MAX_BLOCK_BITS) == 0);
    assert_retnone((page >> shm_config::MAX_PAGE_BITS) == 0);

    size_t key = (block << shm_config::MAX_PAGE_BITS) + page;
    span_map_.set(key, sp);
}

bool page_heap::grow_heap(size_t page_count) {
    sk_assert(shm_config::MAX_PAGES >= shm_config::MIN_HEAP_GROW_PAGE_COUNT);

    if (page_count > shm_config::MAX_PAGE_COUNT)
        return false;

    size_t ask = page_count;
    if (page_count < shm_config::MIN_HEAP_GROW_PAGE_COUNT)
        ask = shm_config::MIN_HEAP_GROW_PAGE_COUNT;

    size_t real_bytes = 0;
    shm_address addr = shm_mgr::allocate(ask << shm_config::PAGE_SHIFT, &real_bytes);
    do {
        if (addr) break;
        if (page_count >= ask) return false;

        ask = page_count;
        addr = shm_mgr::allocate(ask << shm_config::PAGE_SHIFT, &real_bytes);
        if (addr) break;

        return false;
    } while (0);

    sk_assert((real_bytes & shm_config::PAGE_MASK) == 0);
    sk_assert((addr.offset() & shm_config::PAGE_MASK) == 0);

    stat_.grow_count += 1;
    stat_.total_size += real_bytes >> shm_config::PAGE_SHIFT;

    shm_address sp = new_span(addr.block(),
                              addr.offset() >> shm_config::PAGE_SHIFT,
                              real_bytes >> shm_config::PAGE_SHIFT);
    if (!sp) {
        // TODO: handle the allocated memory here...
        // shm_mgr::deallocate(addr, real_bytes);
        sk_assert(0);
        return false;
    }

    span *s = sp.as<span>();
    set_span_map(s->block, s->start_page, sp);
    if (s->page_count > 1)
        set_span_map(s->block, s->start_page + s->page_count - 1, sp);

    s->in_use = 1;

    // in fact, this is actually not an allocation here, however, in
    // deallocate_span() stat_.free_count & stat_.used_size will be
    // changed, to match that, we should also change the corresponding
    // stat_.alloc_count & stat_.used_size here
    ++stat_.alloc_count;
    stat_.used_size += ask;

    deallocate_span(sp);
    return true;
}
