#include "libsk.h"
#include "chunk_mgr.h"
#include "mem_chunk.h"

sk::detail::mem_chunk *sk::detail::lru::__at(int index) {
    assert_retval(index >= 0, NULL);

    mem_chunk *chunk = cast_ptr(mem_chunk, pool + chunk_size * index);
    assert_noeffect(chunk->magic == MAGIC);

    return chunk;
}

void sk::detail::lru::__unlink(int index) {
    assert_retnone(index >= 0);

    mem_chunk *c = __at(index);

    if (c->prev_lru != IDX_NULL)
        __at(c->prev_lru)->next_lru = c->next_lru;
    else {
        assert_noeffect(head == index);
        head = c->next_lru;
    }

    if (c->next_lru != IDX_NULL)
        __at(c->next_lru)->prev_lru = c->prev_lru;
    else {
        assert_noeffect(tail == index);
        tail = c->prev_lru;
    }

    c->prev_lru = IDX_NULL;
    c->next_lru = IDX_NULL;
}

void sk::detail::lru::__push_back(int index) {
    assert_retnone(index >= 0);

    mem_chunk *c = __at(index);
    c->prev_lru = tail;

    if (tail != IDX_NULL)
        __at(tail)->next_lru = index;

    tail = index;

    if (head == IDX_NULL)
        head = index;
}

bool sk::detail::lru::empty() {
    bool empty = (head == IDX_NULL);

    if (empty)
        assert_noeffect(tail == IDX_NULL);
    else
        assert_noeffect(tail != IDX_NULL);

    return empty;
}

int sk::detail::lru::pop() {
    check_retval(head != IDX_NULL, IDX_NULL);

    int index = head;
    __unlink(head);
    return index;
}

int sk::detail::lru::renew(int index) {
    assert_retval(index >= 0, -EINVAL);

    mem_chunk *c = __at(index);

    // 0. in the middle of the list
    if (c->prev_lru != IDX_NULL && c->next_lru != IDX_NULL) {
        __unlink(index);
        __push_back(index);

        return 0;
    }

    // 1. the head
    if (c->next_lru != IDX_NULL) {
        assert_retval(index == head, -EFAULT);
        __unlink(index);
        __push_back(index);

        return 0;
    }

    // 2. the tail, do nothing
    if (c->prev_lru != IDX_NULL) {
        assert_retval(index == tail, -EFAULT);
        return 0;
    }

    // 3. no prev, no next, two possible conditions:
    // 1) this is the only one node in the list, do nothing
    if (index == head) {
        // here head must NOT be IDX_NULL, as index cannot
        // be IDX_NULL here
        assert_retval(index == tail, -EFAULT);
        return 0;
    }

    // 2) not in list, insert it
    __push_back(index);

    return 0;
}

void sk::detail::lru::remove(int index) {
    assert_retnone(index >= 0);

    mem_chunk *c = __at(index);
    if (c->prev_lru == IDX_NULL && c->next_lru == IDX_NULL && index != head)
        return;

    __unlink(index);
}


sk::detail::chunk_mgr *sk::detail::chunk_mgr::create(void *addr, size_t mem_size, bool resume,
                                                     char *pool, size_t pool_size,
                                                     size_t bucket_size, size_t chunk_size, size_t total_chunk_count) {
    assert_retval(addr, NULL);
    assert_retval(mem_size >= calc_size(bucket_size), NULL);
    assert_retval(pool, NULL);
    assert_retval(pool_size >= chunk_size * total_chunk_count, NULL);
    assert_retval(pool + pool_size <= char_ptr(addr) || pool >= char_ptr(addr) + mem_size, NULL);

    chunk_mgr *self = cast_ptr(chunk_mgr, addr);
    char *base_addr = char_ptr(addr);

    base_addr += sizeof(chunk_mgr);
    self->partial_buckets = cast_ptr(int, base_addr);

    base_addr += bucket_size * sizeof(int);
    self->empty_buckets = cast_ptr(int, base_addr);

    self->lru_cache.pool = pool;
    self->pool = pool;

    if (resume) {
        assert_retval(self->chunk_size == chunk_size, NULL);
        assert_retval(self->bucket_size == bucket_size, NULL);
        assert_retval(self->total_chunk_count == total_chunk_count, NULL);
        assert_retval(self->used_chunk_count <= self->total_chunk_count, NULL);
        assert_retval(self->lru_cache.chunk_size == chunk_size, NULL);
    } else {
        self->chunk_size = chunk_size;
        self->bucket_size = bucket_size;
        self->used_chunk_count = 0;
        self->total_chunk_count = total_chunk_count;
        self->lru_cache.head = IDX_NULL;
        self->lru_cache.tail = IDX_NULL;
        self->lru_cache.chunk_size = chunk_size;

        for (size_t i = 0; i < bucket_size; ++i) {
            self->partial_buckets[i] = IDX_NULL;
            self->empty_buckets[i] = IDX_NULL;
        }
    }

    return self;
}

sk::detail::mem_chunk *sk::detail::chunk_mgr::__at(int index, bool check_magic) {
    assert_retval(index >= 0 && static_cast<size_t>(index) < total_chunk_count, NULL);

    mem_chunk *chunk = cast_ptr(mem_chunk, pool + chunk_size * index);
    if (check_magic)
        assert_noeffect(chunk->magic == MAGIC);

    return chunk;
}

void *sk::detail::chunk_mgr::index2ptr(int chunk_index, int block_index) {
    assert_retval(chunk_index >= 0
                   && static_cast<size_t>(chunk_index) < used_chunk_count
                   && block_index >= 0, NULL);

    mem_chunk *chunk = __at(chunk_index);
    return void_ptr(chunk->data + block_index * chunk->block_size);
}

int sk::detail::chunk_mgr::malloc(size_t mem_size, int &chunk_index, int &block_index) {
    size_t idx = mem_size % bucket_size;

    // 0. if there is partially allocated chunk with same size
    if (partial_buckets[idx] != IDX_NULL) {
        chunk_index = partial_buckets[idx];
        mem_chunk *chunk = __at(chunk_index);
        block_index = chunk->malloc();
        assert_retval(block_index >= 0, -EFAULT);

        if (chunk->full()) {
            partial_buckets[idx] = chunk->next_chunk;
            chunk->next_chunk = IDX_NULL;
        }

        return 0;
    }

    // 1. if there is empty chunk with same size
    if (empty_buckets[idx] != IDX_NULL) {
        chunk_index = empty_buckets[idx];
        mem_chunk *chunk = __at(chunk_index);
        block_index = chunk->malloc();
        assert_retval(block_index >= 0, -EFAULT);

        // 0) remove this chunk from empty list, and lru cache
        empty_buckets[idx] = chunk->next_chunk;
        chunk->next_chunk = IDX_NULL;
        lru_cache.remove(chunk_index);

        // 1) if the chunk is not full, add it to partial list
        if (!chunk->full()) {
            chunk->next_chunk = partial_buckets[idx];
            partial_buckets[idx] = chunk_index;
        }

        return 0;
    }

    // 2. if there is unallocated space in the pool
    if (used_chunk_count < total_chunk_count) {
        chunk_index = used_chunk_count;
        mem_chunk *chunk = __at(chunk_index, false);
        int ret = chunk->init(chunk_size, mem_size);
        assert_retval(ret == 0, ret);

        ++used_chunk_count;

        block_index = chunk->malloc();
        assert_retval(block_index >= 0, -EFAULT);

        // add it to partial list if it is not full
        if (!chunk->full()) {
            chunk->next_chunk = partial_buckets[idx];
            partial_buckets[idx] = chunk_index;
        }

        return 0;
    }

    // 3. if there is empty chunk in lru cache (with different size)
    if (!lru_cache.empty()) {
        chunk_index = lru_cache.pop();
        assert_retval(chunk_index != IDX_NULL, -EFAULT);
        mem_chunk *chunk = __at(chunk_index);

        size_t new_idx = chunk->block_size % bucket_size;

        if (empty_buckets[new_idx] == chunk_index) {
            empty_buckets[new_idx] = chunk->next_chunk;
            chunk->next_chunk = IDX_NULL;
        } else {
            mem_chunk *c = __at(empty_buckets[new_idx]);
            bool found = false;
            while (c->next_chunk != IDX_NULL) {
                if (c->next_chunk != chunk_index) {
                    c = __at(c->next_chunk);
                    continue;
                }

                found = true;
                c->next_chunk = chunk->next_chunk;
                chunk->next_chunk = IDX_NULL;
                break;
            }

            assert_noeffect(found);
        }

        int ret = chunk->init(chunk_size, mem_size);
        assert_retval(ret == 0, ret);

        block_index = chunk->malloc();
        assert_retval(block_index >= 0, -EFAULT);

        // add it to partial list if it is not full
        if (!chunk->full()) {
            chunk->next_chunk = partial_buckets[idx];
            partial_buckets[idx] = chunk_index;
        }

        return 0;
    }

    // 4. no partial chunk, no empty chunk, no empty sapce,
    //    then search a bigger size from partial list
    while (true) {
        ++idx;

        // no bigger partial chunk found, break
        check_break(idx < bucket_size);

        // current bucket is an invalid bucket, continue to search
        check_continue(partial_buckets[idx] != IDX_NULL);

        chunk_index = partial_buckets[idx];
        mem_chunk *chunk = __at(chunk_index);
        block_index = chunk->malloc();
        assert_retval(block_index >= 0, -EFAULT);

        if (chunk->full()) {
            partial_buckets[idx] = chunk->next_chunk;
            chunk->next_chunk = IDX_NULL;
        }

        return 0;
    }

    // no proper chunk found, out of memory :(
    ERR("no more space :(");
    return -ENOMEM;
}

void sk::detail::chunk_mgr::free(int chunk_index, int block_index) {
    assert_retnone(chunk_index >= 0
                   && static_cast<size_t>(chunk_index) < used_chunk_count
                   && block_index >= 0);

    mem_chunk *chunk = __at(chunk_index);

    bool full_before_free = chunk->full();
    chunk->free(block_index);
    bool empty_after_free = chunk->empty();

    const size_t idx = chunk->block_size % bucket_size;

    // 0. full before free, empty after free, add to empty list, and lru cache
    if (full_before_free && empty_after_free) {
        chunk->next_chunk = empty_buckets[idx];
        empty_buckets[idx] = chunk_index;
        lru_cache.renew(chunk_index);

        return;
    }

    // 1. full before free, not empty after free, add to partial list
    if (full_before_free) {
        chunk->next_chunk = partial_buckets[idx];
        partial_buckets[idx] = chunk_index;

        return;
    }

    // 2. not full before, but empty after free, remove from partial list,
    //    then add to empty list and lru cache
    if (empty_after_free) {
        if (partial_buckets[idx] == chunk_index) {
            partial_buckets[idx] = chunk->next_chunk;
            chunk->next_chunk = IDX_NULL;
        } else {
            mem_chunk *c = __at(partial_buckets[idx]);
            bool found = false;
            while (c->next_chunk != IDX_NULL) {
                if (c->next_chunk != chunk_index) {
                    c = __at(c->next_chunk);
                    continue;
                }

                found = true;
                c->next_chunk = chunk->next_chunk;
                chunk->next_chunk = IDX_NULL;
                break;
            }

            assert_noeffect(found);
        }

        chunk->next_chunk = empty_buckets[idx];
        empty_buckets[idx] = chunk_index;
        lru_cache.renew(chunk_index);

        return;
    }

    // 3. not full before free, also not empty after free, it will still
    //    lay in partial list, so we do nothing here
}
