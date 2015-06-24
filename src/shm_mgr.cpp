#include "sk_inc.h"
#include "detail/buddy.h"
#include "detail/hash.h"
#include "detail/mem_chunk.h"
#include "detail/stack.h"

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

typedef sk::fixed_array<singleton_info, sk::ST_MAX> singleton_array;
static singleton_array singleton_meta_info;

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

int sk::shm_mgr_init(key_t main_key, key_t aux_key1, key_t aux_key2, key_t aux_key3,
                     bool resume, size_t max_block_size, int chunk_count, size_t heap_size) {
    mgr = sk::shm_mgr::create(main_key, aux_key1, aux_key2, aux_key3,
                              resume, max_block_size, chunk_count, heap_size);

    if (!mgr)
        return -EINVAL;

    return 0;
}

int sk::shm_mgr_fini() {
    if (!mgr)
        return 0;

    shmctl(mgr->heap->shmid, IPC_RMID, 0);
    shmctl(mgr->empty_chunk_stack->shmid, IPC_RMID, 0);
    shmctl(mgr->free_chunk_hash->shmid, IPC_RMID, 0);
    shmctl(mgr->shmid, IPC_RMID, 0);

    mgr = NULL;
    return 0;
}

inline bool sk::shm_mgr::__power_of_two(size_t num) {
    return !(num & (num - 1));
}

inline size_t sk::shm_mgr::__fix_size(size_t size) {
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

sk::shm_mgr *sk::shm_mgr::create(key_t main_key, key_t aux_key1, key_t aux_key2, key_t aux_key3,
                                 bool resume, size_t max_block_size, int chunk_count, size_t heap_size) {
    max_block_size = __align_size(max_block_size);
    size_t chunk_size = max_block_size + sizeof(mem_chunk);

    // for blocks with size > max_block_size will go into heap
    size_t heap_unit_size = __align_size(max_block_size);
    size_t heap_unit_count = heap_size / heap_unit_size;

    // heap unit count must be 2^n
    heap_unit_count = __fix_size(heap_unit_count);
    heap_size  = heap_unit_count * heap_unit_size;

    size_t singleton_size = 0;
    for (singleton_array::iterator it = singleton_meta_info.begin(); it != singleton_meta_info.end(); ++it)
        singleton_size += it->size;

    size_t shm_size = 0;
    shm_size += sizeof(shm_mgr);
    shm_size += singleton_size;
    shm_size += chunk_size * chunk_count;
    shm_size += heap_size;

    sk::shm_seg seg;
    int ret = seg.init(main_key, shm_size, resume);
    if (ret != 0) {
        ERR("cannot create shm mgr, key<%d>, size<%lu>.", main_key, shm_size);
        return NULL;
    }

    shm_mgr *self = NULL;
    if (resume)
        self = static_cast<shm_mgr *>(seg.address());
    else
        self = static_cast<shm_mgr *>(seg.malloc(sizeof(shm_mgr)));

    if (!self) {
        ERR("memory error.");
        seg.free();
        return NULL;
    }

    char *base_addr = static_cast<char *>(static_cast<void *>(self));
    self->pool = base_addr + sizeof(*self) + singleton_size;

    if (!resume) {
        self->shmid = seg.shmid;
        self->chunk_size = chunk_size;
        self->max_block_size = max_block_size;

        self->heap_total_size = heap_size;
        self->heap_unit_size = heap_unit_size;

        {
            char *singleton_base = base_addr + sizeof(*self);
            memset(self->singletons, 0x00, sizeof(self->singletons));

            for (singleton_array::iterator it = singleton_meta_info.begin(); it != singleton_meta_info.end(); ++it) {
                assert_retval(it->id >= ST_MIN && it->id < ST_MAX, NULL);
                self->singletons[it->id] = singleton_base - char_ptr(self);
                singleton_base += it->size;
            }

            assert_retval(singleton_base == self->pool, NULL);
        }

        self->pool_head_offset = self->pool - base_addr;
        self->pool_end_offset  = shm_size;

        self->chunk_end = 0;
        self->heap_head = chunk_size * chunk_count;
    }

    // 1. init hash
    {
        int node_count = chunk_count;
        int hash_size  = chunk_size - sizeof(mem_chunk); // max block size
        self->free_chunk_hash = size_ptr_hash::create(aux_key1, resume, node_count, hash_size);
        if (!self->free_chunk_hash) {
            ERR("cannot create free chunk hash.");
            seg.free();
            return NULL;
        }
    }

    // 2. init stack
    {
        int node_count = chunk_count;
        self->empty_chunk_stack = stack::create(aux_key2, resume, node_count);
        if (!self->empty_chunk_stack) {
            ERR("cannot create empty chunk stack.");
            seg.free();
            return NULL;
        }
    }

    // 3. init heap
    {
        int unit_count = static_cast<int>(heap_unit_count);
        self->heap = heap_allocator::create(aux_key3, resume, unit_count);
        if (!self->heap) {
            ERR("cannot create heap.");
            seg.free();
            return NULL;
        }
    }

    return self;
}

sk::shm_mgr *sk::shm_mgr::get() {
    assert_retval(mgr, NULL);
    return mgr;
}

void *sk::shm_mgr::offset2ptr(offset_t offset) {
    assert_retval(offset >= sizeof(*this), NULL);

    return static_cast<void *>(static_cast<char *>(static_cast<void *>(this)) + offset);
}

sk::shm_mgr::offset_t sk::shm_mgr::ptr2offset(void *ptr) {
    ptrdiff_t offset = static_cast<char *>(ptr) - static_cast<char *>(static_cast<void *>(this));
    assert_retval(offset >= sizeof(*this), INVALID_OFFSET);

    return offset;
}

inline sk::shm_mgr::mem_chunk *sk::shm_mgr::__offset2chunk(offset_t offset) {
    void *p = offset2ptr(offset);
    assert_retval(p, NULL);
    assert_retval((ptr - pool_head_offset) % chunk_size == 0, NULL);

    return static_cast<mem_chunk *>(p);
}

inline sk::shm_mgr::offset_t sk::shm_mgr::__chunk2offset(sk::shm_mgr::mem_chunk *chunk) {
    assert_retval(chunk, INVALID_OFFSET);

    return ptr2offset(static_cast<void *>(chunk));
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
            idx = empty_chunk_stack->pop();
            assert_retval(idx, -EFAULT);

            // TODO: check if chunk is null here, and also
            // verify the return value of chun->init(...)
            // also check item 3
            chunk = __index2chunk(*idx);
            chunk->init(chunk_size, mem_size);
            index = *idx;
            break;

            // TODO: here if the chunk has only one block,
            // then there is no need to insert it into the
            // hash, as we will remove it from the hash later
            // this condition also applies to item 3
            int ret = free_chunk_hash->insert(mem_size, *idx);
            assert_retval(ret == 0, -EFAULT);

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
        int ret = free_chunk_hash->erase(chunk->block_size, chunk_index);
        assert_retnone(ret == 0);

        ret = empty_chunk_stack->push(chunk_index);
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

void *sk::shm_mgr::get_singleton(int id) {
    assert_retval(id >= ST_MIN && id < ST_MAX, NULL);

    shm_ptr ptr = singletons[id];
    assert_retval(ptr != SHM_NULL, NULL);

    return ptr2ptr(ptr);
}


void *sk::shm_malloc(size_t size, shm_ptr &ptr) {
    void *raw_ptr = NULL;
    shm_mgr *mgr = shm_mgr::get();
    assert_retval(mgr, NULL);

    ptr = mgr->malloc(size, raw_ptr);
    // the two pointers must either be null or not null simultaneously
    assert_retval((ptr == SHM_NULL && raw_ptr == NULL) || (ptr != SHM_NULL && raw_ptr != NULL), NULL);

    return raw_ptr;
}

void sk::shm_free(shm_ptr ptr) {
    shm_mgr *mgr = shm_mgr::get();
    assert_retnone(mgr);

    mgr->free(ptr);
}

void sk::shm_free(void *ptr) {
    shm_mgr *mgr = shm_mgr::get();
    assert_retnone(mgr);

    mgr->free(ptr);
}

void *sk::shm_singleton(int id) {
    shm_mgr *mgr = shm_mgr::get();
    assert_retval(mgr, NULL);

    return mgr->get_singleton(id);
}
