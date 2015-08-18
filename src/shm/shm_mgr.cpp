#include "libsk.h"
#include "detail/buddy.h"
#include "detail/mem_chunk.h"

struct singleton_info {
    int id;
    size_t size;

    bool operator==(const singleton_info& other) {
        if (this == &other)
            return true;

        if (other.id == this->id)
            return true;

        return false;
    }
};

typedef sk::fixed_vector<singleton_info, sk::ST_MAX> singleton_vector;
static singleton_vector singleton_meta_info;

static sk::shm_mgr *mgr = NULL;


int sk::register_singleton(int id, size_t size) {
    assert_retval(id >= ST_MIN && id < ST_MAX, -EINVAL);

    singleton_info tmp;
    tmp.id = id;
    assert_retval(!singleton_meta_info.find(tmp), -EEXIST);

    singleton_info *info = singleton_meta_info.emplace();
    assert_retval(info, -ENOMEM);

    info->id = id;
    // TODO: we may fix the size here to make it aligned
    info->size = size;

    return 0;
}

int sk::shm_mgr_init(key_t main_key, bool resume,
                     size_t max_block_size, int chunk_count, size_t heap_size) {
    mgr = sk::shm_mgr::create(main_key, resume, max_block_size, chunk_count, heap_size);

    if (!mgr)
        return -EINVAL;

    return 0;
}

int sk::shm_mgr_fini() {
    if (!mgr)
        return 0;

    shmctl(mgr->shmid, IPC_RMID, 0);

    mgr = NULL;
    return 0;
}

inline bool sk::shm_mgr::__power_of_two(size_t num) {
    return !(num & (num - 1));
}

inline size_t sk::shm_mgr::__fix_size(size_t size) {
    static_assert(sizeof(size) == 8, "only 64 bit size can be fixed with this function");

    if (__power_of_two(size))
        return size;

    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    size |= size >> 32;
    return size + 1;
}

inline size_t sk::shm_mgr::__align_size(size_t size) {
    bool has_lo = false;
    if (size & ALIGN_MASK)
        has_lo = true;

    size &= ~ALIGN_MASK;
    if (has_lo)
        size += ALIGN_SIZE;

    return size;
}

sk::shm_mgr *sk::shm_mgr::create(key_t main_key, bool resume,
                                 size_t max_block_size, int chunk_count, size_t heap_size) {
    max_block_size = __align_size(max_block_size);
    size_t chunk_size = max_block_size + sizeof(mem_chunk);

    // for blocks with size > max_block_size will go into heap
    size_t heap_unit_size = __align_size(max_block_size);
    size_t heap_unit_count = heap_size / heap_unit_size;
    // heap unit count must be 2^n
    heap_unit_count = __fix_size(heap_unit_count);
    heap_size  = heap_unit_count * heap_unit_size;

    size_t singleton_size = 0;
    for (auto it = singleton_meta_info.begin(); it != singleton_meta_info.end(); ++it)
        singleton_size += it->size;

    size_t hash_size = size_index_hash::calc_size(chunk_count, max_block_size);
    size_t stack_size = stack::calc_size(chunk_count);
    size_t heap_allocator_size = heap_allocator::calc_size(heap_unit_count);

    size_t shm_size = 0;
    shm_size += sizeof(shm_mgr);
    shm_size += singleton_size;
    shm_size += hash_size;
    shm_size += stack_size;
    shm_size += heap_allocator_size;
    shm_size += chunk_size * chunk_count;
    shm_size += heap_size;

    sk::shm_seg seg;
    int ret = seg.init(main_key, shm_size, resume);
    if (ret != 0) {
        ERR("cannot create shm mgr, key<%d>, size<%lu>.", main_key, shm_size);
        return NULL;
    }

    shm_mgr *self = cast_ptr(shm_mgr, seg.address());
    char *base_addr = char_ptr(seg.address());
    base_addr += sizeof(shm_mgr);

    // 0. init singletons
    {
        if (resume)
            base_addr += singleton_size;
        else {
            memset(self->singletons, 0x00, sizeof(self->singletons));

            for (auto it = singleton_meta_info.begin(); it != singleton_meta_info.end(); ++it) {
                assert_retval(it->id >= ST_MIN && it->id < ST_MAX, NULL);
                self->singletons[it->id] = base_addr - char_ptr(self);
                base_addr += it->size;
            }
        }
    }

    // 1. init free_chunk_hash
    {
        self->free_chunk_hash = size_index_hash::create(base_addr, hash_size, resume, chunk_count, max_block_size);
        assert_retval(self->free_chunk_hash, NULL);

        base_addr += hash_size;
    }

    // 2. init empty_stack
    {
        self->empty_chunk_stack = stack::create(base_addr, stack_size, resume, chunk_count);
        assert_retval(self->empty_chunk_stack, NULL);

        base_addr += stack_size;
    }

    // 3. init heap allocator
    {
        self->heap = heap_allocator::create(base_addr, heap_allocator_size, resume, heap_unit_count);
        assert_retval(self->heap, NULL);

        base_addr += heap_allocator_size;
    }

    self->pool = base_addr;

    if (resume) {
        assert_noeffect(self->shmid == seg.shmid);
        assert_noeffect(self->chunk_size == chunk_size);
        assert_noeffect(self->max_block_size == max_block_size);
        assert_noeffect(self->heap_total_size == heap_size);
        assert_noeffect(self->heap_unit_size == heap_unit_size);
    } else {
        self->shmid = seg.shmid;

        self->chunk_size = chunk_size;
        self->max_block_size = max_block_size;

        self->heap_total_size = heap_size;
        self->heap_unit_size = heap_unit_size;

        self->pool_head_offset = self->pool - char_ptr(self);
        self->pool_end_offset  = shm_size;

        self->chunk_end = 0;
        self->heap_head = chunk_size * chunk_count;
    }

    seg.release();
    return self;
}

sk::shm_mgr *sk::shm_mgr::get() {
    assert_retval(mgr, NULL);
    return mgr;
}

sk::shm_mgr::mem_chunk *sk::shm_mgr::__index2chunk(sk::shm_mgr::index_t idx) {
    assert_retval(idx >= 0, NULL);

    size_t offset = idx * chunk_size;
    assert_retval(offset + chunk_size <= chunk_end, NULL);

    return cast_ptr(mem_chunk, pool + offset);
}

int sk::shm_mgr::__malloc_from_chunk_pool(size_t mem_size, int &chunk_index, int &block_index) {
    mem_chunk *chunk = NULL;
    index_t index = IDX_NULL;

    do {
        index_t *idx = free_chunk_hash->find(mem_size);

        // 1. there exists free chunk with same size
        if (idx) {
            chunk = __index2chunk(*idx);
            index = *idx;
            break;
        }

        // 2. no free chunk exists, but there is empty chunk
        if (!empty_chunk_stack->empty()) {
            idx = empty_chunk_stack->top();
            assert_retval(idx, -EFAULT);

            // TODO: check if chunk is null here, and also
            // verify the return value of chun->init(...)
            // also check item 3
            chunk = __index2chunk(*idx);
            chunk->init(chunk_size, mem_size);
            index = *idx;

            // TODO: here if the chunk has only one block,
            // then there is no need to insert it into the
            // hash, as we will remove it from the hash later
            // this condition also applies to item 3
            int ret = free_chunk_hash->insert(mem_size, *idx);
            assert_retval(ret == 0, -EFAULT);

            empty_chunk_stack->pop();
            break;
        }

        // 3. there is available chunk in pool
        if (chunk_end + chunk_size <= heap_head) {
            assert_retval(chunk_end % chunk_size == 0, -EFAULT);

            chunk = cast_ptr(mem_chunk, pool + chunk_end);
            chunk->init(chunk_size, mem_size);

            index = chunk_end / chunk_size;
            int ret = free_chunk_hash->insert(mem_size, index);
            assert_retval(ret == 0, -EFAULT);

            chunk_end += chunk_size;
            break;
        }

        // 4. no empty chunk, and also chunk pool has used up
        //    we hope it will never get here :(
        ERR("chunk pool has been used up!!!");
        return -ENOMEM;

    } while (0);

    assert_retval(chunk, -EFAULT);

    // actually, it should never get here
    if (chunk->full()) {
        assert_noeffect(0);

        // however, if it gets here, we still can handle:
        // remove the full chunk, and call this function recursively
        free_chunk_hash->erase(mem_size);
        return __malloc_from_chunk_pool(mem_size, chunk_index, block_index);
    }

    block_index = chunk->malloc();
    assert_retval(block_index >= 0, -EFAULT);

    chunk_index = index;

    if (chunk->full())
        free_chunk_hash->erase(mem_size);

    return 0;
}

int sk::shm_mgr::__malloc_from_heap(size_t mem_size, int &unit_index) {
    u32 unit_count = mem_size / heap_unit_size;
    if (mem_size % heap_unit_size != 0)
        unit_count += 1;

    int idx = heap->malloc(unit_count);

    // there is no more memory
    // we hope it will never get here :(
    if (idx < 0) {
        ERR("no more space on heap!!!");
        return -ENOMEM;
    }

    unit_index = idx;
    return 0;
}

void sk::shm_mgr::__free_from_chunk_pool(int chunk_index, int block_index) {
    assert_retnone(chunk_index >= 0 && block_index >= 0);
    size_t chunk_offset = chunk_size * chunk_index;
    assert_retnone(chunk_offset + chunk_size <= chunk_end);

    mem_chunk *chunk = cast_ptr(mem_chunk, pool + chunk_offset);

    bool full_before_free = chunk->full();

    chunk->free(block_index);

    bool empty_after_free = chunk->empty();

    if (full_before_free && empty_after_free) {
        int ret = empty_chunk_stack->push(chunk_index);
        assert_retnone(ret == 0);
    } else if (full_before_free) {
        int ret = free_chunk_hash->insert(chunk->block_size, chunk_index);
        assert_retnone(ret == 0);
    } else if (empty_after_free) {
        free_chunk_hash->erase(chunk->block_size, chunk_index);

        int ret = empty_chunk_stack->push(chunk_index);
        assert_retnone(ret == 0);
    } else {
        // not full before free, also not empty after free, it
        // will still lay in free_chunk_hash, so we do nothing here
    }
}

void sk::shm_mgr::__free_from_heap(int unit_index) {
    assert_retnone(unit_index >= 0);

    heap->free(unit_index);
}

// TODO: refine this function, it has almost the same logic with free(...)
void *sk::shm_mgr::mid2ptr(u64 mid) {
    if (!mid)
        return NULL;

    detail::detail_ptr *ptr = cast_ptr(detail::detail_ptr, &mid);
    switch (ptr->type) {
    case detail::PTR_TYPE_SINGLETON: {
        int singleton_id = ptr->idx1;
        assert_retval(singleton_id >= ST_MIN && singleton_id < ST_MAX, NULL);

        return get_singleton(singleton_id);
    }
    case detail::PTR_TYPE_CHUNK: {
        int chunk_index = ptr->idx1;
        int block_index = ptr->idx2;
        assert_retval(chunk_index >= 0 && block_index >= 0, NULL);

        // TODO: extract the logic below into a method of mem_chunk
        mem_chunk *chunk = __index2chunk(chunk_index);
        assert_retval(chunk && chunk->magic == MAGIC, NULL);

        return void_ptr(chunk->data + block_index * chunk->block_size);
    }
    case detail::PTR_TYPE_HEAP: {
        int unit_index = ptr->idx1;
        assert_retval(unit_index >= 0, NULL);

        // TODO: define another field: "char *heap_pool", seperate "chunk_pool" and "heap_pool"
        return void_ptr(pool + heap_head + unit_index * heap_unit_size);
    }
    default:
        assert_retval(0, NULL);
    }
}

void *sk::shm_mgr::get_singleton(int id) {
    assert_retval(id >= ST_MIN && id < ST_MAX, NULL);

    offset_t offset = singletons[id];
    assert_retval(offset >= sizeof(*this), NULL);

    return void_ptr(char_ptr(this) + offset);
}

sk::shm_ptr<void> sk::shm_mgr::malloc(size_t size) {
    size_t mem_size = __align_size(size);

    do {
        if (mem_size > max_block_size)
            break;

        int chunk_index = IDX_NULL;
        int block_index = IDX_NULL;
        int ret = __malloc_from_chunk_pool(mem_size, chunk_index, block_index);

        // 1. the allocation succeeds
        if (ret == 0) {
            detail::detail_ptr ptr;
            ptr.type = detail::PTR_TYPE_CHUNK;
            ptr.idx1 = chunk_index;
            ptr.idx2 = block_index;

            return shm_ptr<void>(ptr);
        }

        // 2. fatal errors other than "no more memory", we return NULL
        if (ret != -ENOMEM)
            return shm_ptr<void>();

        // 3. the error is because chunk pool is used up, then we will
        //    allocate it from heap
        break;

    } while (0);

    // if the block should be allocated on heap, or the allocation fails in chunk pool
    // because it is used up, then we do the allocation on heap
    int unit_index = IDX_NULL;
    int ret = __malloc_from_heap(mem_size, unit_index);
    if (ret != 0)
        return shm_ptr<void>();

    detail::detail_ptr ptr;
    ptr.type = detail::PTR_TYPE_HEAP;
    ptr.idx1 = unit_index;
    ptr.idx2 = 0;

    return shm_ptr<void>(ptr);
}

void sk::shm_mgr::free(shm_ptr<void> ptr) {
    if (!ptr)
        return;

    detail::detail_ptr *raw_ptr = cast_ptr(detail::detail_ptr, &ptr.mid);
    switch (raw_ptr->type) {
    case detail::PTR_TYPE_SINGLETON: {
        int singleton_id = raw_ptr->idx1;
        assert_retnone(singleton_id >= ST_MIN && singleton_id < ST_MAX);

        assert_noeffect(0);
        return;
    }
    case detail::PTR_TYPE_CHUNK: {
        int chunk_index = raw_ptr->idx1;
        int block_index = raw_ptr->idx2;
        assert_retnone(chunk_index >= 0 && block_index >= 0);

        __free_from_chunk_pool(chunk_index, block_index);
        return;
    }
    case detail::PTR_TYPE_HEAP: {
        int unit_index = raw_ptr->idx1;
        assert_retnone(unit_index >= 0);

        __free_from_heap(unit_index);
        return;
    }
    default:
        assert_retnone(0);
    }
}


sk::shm_ptr<void> sk::shm_malloc(size_t size) {
    shm_mgr *mgr = shm_mgr::get();
    assert_retval(mgr, shm_ptr<void>());

    return mgr->malloc(size);
}

void sk::shm_free(shm_ptr<void> ptr) {
    shm_mgr *mgr = shm_mgr::get();
    assert_retnone(mgr);

    mgr->free(ptr);
}

void *sk::shm_singleton(int id) {
    shm_mgr *mgr = shm_mgr::get();
    assert_retval(mgr, NULL);

    return mgr->get_singleton(id);
}
