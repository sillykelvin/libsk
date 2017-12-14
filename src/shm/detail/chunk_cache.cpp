#include <shm/shm.h>
#include <shm/detail/page_heap.h>
#include <utility/assert_helper.h>
#include <shm/detail/chunk_cache.h>

using namespace sk::detail;

shm_address chunk_cache::allocate_chunk(size_t bytes, u8 sc) {
    assert_retval(sc < array_len(caches_), nullptr);

    const size_t old_bytes = bytes;
    bytes = shm_size_map()->class2size(sc);
    sk_assert(bytes >= old_bytes);

    shm_address head = caches_[sc].free_list.addr();
    shm_address sp = nullptr;
    span *s = nullptr;

    do {
        // check if there is available chunk in the specified class cache
        if (!span_list_empty(head)) {
            sk_assert(caches_[sc].span_count > 0);
            sp = head.as<span>()->next();
            s = sp.as<span>();
            break;
        }

        // no available chunk, then we fetch a span from page heap
        size_t page_count = shm_size_map()->class2pages(sc);
        sp = shm_page_heap()->allocate_span(page_count);

        // span allocation failure, we can't help here
        if (!sp) return nullptr;

        s = sp.as<span>();
        ++stat_.span_alloc_count;
        stat_.total_size += page_count << shm_config::PAGE_BITS;
        shm_page_heap()->register_span(sp);
        s->partition(bytes, sc);
        span_list_prepend(head, sp);
        ++caches_[sc].span_count;
        caches_[sc].stat.total_size += page_count << shm_config::PAGE_BITS;
    } while (0);

    shm_address ret = s->fetch();
    assert_retval(ret, nullptr);

    // the span is full, unlink it from the free list
    if (!s->chunk_list()) {
        span_list_remove(sp);
        --caches_[sc].span_count;
        // do NOT change any stat memeber of caches_[sc] here
    }

    ++caches_[sc].stat.alloc_count;
    ++stat_.alloc_count;
    caches_[sc].stat.used_size += bytes;
    stat_.used_size += bytes;

    return ret;
}

void chunk_cache::deallocate_chunk(shm_address addr, shm_address sp) {
    assert_retnone(addr);
    assert_retnone(sp);

    span *s = sp.as<span>();
    assert_retnone(s->used_count() > 0);

    const u8 sc = cast_u8(s->size_class());
    assert_retnone(sc < array_len(caches_));

    shm_address head = caches_[sc].free_list.addr();

    const bool full_before = !s->chunk_list();
    s->recycle(addr);
    const bool empty_after = s->used_count() <= 0;

    if (!full_before) {
        if (empty_after) {
            // this span is still in the list, so we compare with 1 here
            if (caches_[sc].span_count > 1) {
                span_list_remove(sp);
                --caches_[sc].span_count;

                const size_t bytes = s->page_count() << shm_config::PAGE_BITS;
                sk_assert(caches_[sc].stat.total_size >= bytes);
                sk_assert(stat_.total_size >= bytes);

                caches_[sc].stat.total_size -= bytes;
                stat_.total_size -= bytes;
                ++stat_.span_free_count;

                s->erase();
                shm_page_heap()->deallocate_span(sp);
            } else {
                // do nothing here as it's the only span in this class cache
            }
        } else {
            // do nothing here as span is not empty
        }
    } else {
        if (empty_after) {
            // this span is not in the list, se we compare with 0 here
            if (caches_[sc].span_count > 0) {
                const size_t bytes = s->page_count() << shm_config::PAGE_BITS;
                sk_assert(caches_[sc].stat.total_size >= bytes);
                sk_assert(stat_.total_size >= bytes);

                caches_[sc].stat.total_size -= bytes;
                stat_.total_size -= bytes;
                ++stat_.span_free_count;

                s->erase();
                shm_page_heap()->deallocate_span(sp);
            } else {
                span_list_prepend(head, sp);
                ++caches_[sc].span_count;
            }
        } else {
            span_list_prepend(head, sp);
            ++caches_[sc].span_count;
        }
    }

    const size_t bytes = shm_size_map()->class2size(sc);
    sk_assert(caches_[sc].stat.used_size >= bytes);
    sk_assert(stat_.used_size >= bytes);

    caches_[sc].stat.used_size -= bytes;
    stat_.used_size -= bytes;
    ++caches_[sc].stat.free_count;
    ++stat_.free_count;
}
