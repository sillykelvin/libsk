#include <shm/shm.h>
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
    sk_assert(s->in_use());
    sk_assert(s->page_count() > 0);
    sk_assert(!s->prev());
    sk_assert(!s->next());
    sk_assert(get_span_map(s->start_page()) == sp);
    sk_assert(get_span_map(s->last_page()) == sp);

    s->set_in_use(false);

    const shm_page_t orig_start = s->start_page();
    const size_t     orig_count = s->page_count();

    shm_address prev = nullptr;
    if (orig_start > 0) {
        prev = get_span_map(orig_start - 1);
        if (prev) {
            span *p = prev.as<span>();
            if (!p->in_use()) {
                sk_assert(p->page_count() > 0);
                sk_assert(p->start_page() + p->page_count() == orig_start);

                s->set_start_page(s->start_page() - p->page_count());
                s->set_page_count(s->page_count() + p->page_count());

                span_list_remove(prev);
                del_span(prev);
                set_span_map(s->start_page(), sp);
            }
        }
    }

    shm_address next = nullptr;
    if (orig_start + orig_count < shm_config::MAX_PAGE_COUNT) {
        next = get_span_map(orig_start + orig_count);
        if (next) {
            span *n = next.as<span>();
            if (!n->in_use()) {
                sk_assert(n->page_count() > 0);
                sk_assert(n->start_page() - orig_count == orig_start);

                s->set_page_count(s->page_count() + n->page_count());

                span_list_remove(next);
                del_span(next);
                set_span_map(s->last_page(), sp);
            }
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
    sk_assert(s->in_use());
    sk_assert(get_span_map(s->start_page()) == sp);
    sk_assert(get_span_map(s->last_page()) == sp);

    size_t last = s->page_count() - 1;
    for (size_t i = 1; i < last; ++i)
        set_span_map(s->start_page() + i, sp);
}

shm_address page_heap::find_span(shm_address addr) {
    return get_span_map(addr.offset() >> shm_config::PAGE_BITS);
}

shm_address page_heap::search_existing(size_t page_count) {
    for (size_t i = page_count; i < shm_config::MAX_PAGES; ++i) {
        shm_address head = free_lists_[i].addr();
        if (!span_list_empty(head))
            return carve(head.as<span>()->next(), page_count);
    }

    return allocate_large(page_count);
}

shm_address page_heap::allocate_large(size_t page_count) {
    shm_address best(nullptr);

    shm_address it  = large_list_.next();
    shm_address end = large_list_.addr();
    while (it != end) {
        span *s = it.as<span>();
        if (s->page_count() < page_count) {
            it = s->next();
            continue;
        }

        if (!best) {
            best = it;
            it = s->next();
            continue;
        }

        span *b = best.as<span>();
        if (s->page_count() < b->page_count() ||
           (s->page_count() == b->page_count() && s->start_page() < b->start_page())) {
            best = it;
            it = s->next();
            continue;
        }

        it = s->next();
    }

    return best ? carve(best, page_count) : nullptr;
}

shm_address page_heap::carve(shm_address sp, size_t page_count) {
    span *s = sp.as<span>();
    assert_retval(!s->in_use(), nullptr);
    assert_retval(s->page_count() >= page_count, nullptr);

    span_list_remove(sp);

    const size_t extra = s->page_count() - page_count;
    if (extra > 0) {
        shm_address left = new_span(s->start_page() + page_count, extra);
        if (left) {
            span *l = left.as<span>();

            set_span_map(l->start_page(), left);
            if (l->page_count() > 1)
                set_span_map(l->last_page(), left);

            shm_address next = get_span_map(l->start_page() + l->page_count());
            sk_assert(!next || next.as<span>()->in_use());

            link(left);
            s->set_page_count(page_count);
            set_span_map(s->last_page(), sp);
        }
    }

    s->set_in_use(true);
    return sp;
}

void page_heap::link(shm_address sp) {
    span *s = sp.as<span>();
    size_t page_count = s->page_count();
    span *head = (page_count < shm_config::MAX_PAGES) ? &free_lists_[page_count] : &large_list_;

    span_list_prepend(head->addr(), sp);
}

shm_address page_heap::new_span(shm_page_t start_page, size_t page_count) {
    shm_address sp = span_allocator_.allocate();
    if (sp) {
        span *s = sp.as<span>();
        new (s) span(start_page, page_count);
    }

    return sp;
}

void page_heap::del_span(shm_address sp) {
    // TODO: shouldn't we call span::~span() here?
    // as we called span::span(...) in new_span(...)
    span_allocator_.deallocate(sp);
}

shm_address page_heap::get_span_map(shm_page_t page) const {
    assert_retval(page < shm_config::MAX_PAGE_COUNT, nullptr);

    const shm_address *addr = span_map_.get(page);
    return addr ? *addr : nullptr;
}

void page_heap::set_span_map(shm_page_t page, shm_address sp) {
    assert_retnone(page < shm_config::MAX_PAGE_COUNT);
    span_map_.set(page, sp);
}

bool page_heap::grow_heap(size_t page_count) {
    sk_assert(shm_config::MAX_PAGES >= shm_config::HEAP_GROW_PAGE_COUNT);
    check_retval(page_count <= shm_config::MAX_PAGE_COUNT, false);

    size_t ask = page_count;
    if (page_count < shm_config::HEAP_GROW_PAGE_COUNT)
        ask = shm_config::HEAP_GROW_PAGE_COUNT;

    size_t real_bytes = ask << shm_config::PAGE_BITS;
    shm_address addr = shm_allocate_userdata(&real_bytes);
    if (!addr) {
        if (page_count >= ask) return false;

        ask = page_count;
        real_bytes = ask << shm_config::PAGE_BITS;
        addr = shm_allocate_userdata(&real_bytes);
        if (!addr) return false;
    }

    sk_assert((real_bytes & shm_config::PAGE_MASK) == 0);
    sk_assert((addr.offset() & shm_config::PAGE_MASK) == 0);
    sk_assert(addr.serial() == shm_config::USERDATA_SERIAL_NUM);

    stat_.grow_count += 1;
    stat_.total_size += real_bytes >> shm_config::PAGE_BITS;

    shm_address sp = new_span(addr.offset() >> shm_config::PAGE_BITS,
                              real_bytes >> shm_config::PAGE_BITS);
    if (!sp) {
        // TODO: handle the allocated memory here...
        // shm_mgr::deallocate(addr, real_bytes);
        sk_assert(0);
        return false;
    }

    span *s = sp.as<span>();
    set_span_map(s->start_page(), sp);
    if (s->page_count() > 1)
        set_span_map(s->last_page(), sp);

    s->set_in_use(true);

    // in fact, this is actually not an allocation here, however, in
    // deallocate_span() stat_.free_count & stat_.used_size will be
    // changed, to match that, we should also change the corresponding
    // stat_.alloc_count & stat_.used_size here
    ++stat_.alloc_count;
    stat_.used_size += ask;

    deallocate_span(sp);
    return true;
}
