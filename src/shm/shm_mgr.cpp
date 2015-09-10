#include "libsk.h"
#include "detail/buddy.h"
#include "detail/chunk_mgr.h"
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


int sk::shm_register_singleton(int id, size_t size) {
    assert_retval(id >= ST_MIN && id < ST_MAX, -EINVAL);
    // if mgr is inited, the registration will be meaningless
    assert_retval(!mgr, -EINVAL);

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

int sk::shm_mgr_init(key_t key, bool resume,
                     size_t max_block_size, int chunk_count, size_t heap_size) {
    mgr = sk::shm_mgr::create(key, resume, max_block_size, chunk_count, heap_size);

    return mgr ? 0 : -EINVAL;
}

int sk::shm_mgr_fini() {
    if (!mgr)
        return 0;

    // TODO: we may need to call destructors of the objects
    // still in pool here
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
    // TODO: redefine the ALIGN related macros
    return (size + ALIGN_MASK) & ~ALIGN_MASK;
}

sk::shm_mgr *sk::shm_mgr::create(key_t key, bool resume,
                                 size_t max_block_size, int chunk_count, size_t heap_size) {
    DBG("shm mgr creation parameters: max_block_size<%lu>, chunk_count<%d>, heap_size<%lu>.",
        max_block_size, chunk_count, heap_size);

    // TODO: refine the size fixing here
    max_block_size = __align_size(max_block_size);
    size_t chunk_size = max_block_size + sizeof(mem_chunk);

    // for blocks with size > max_block_size will go into heap
    size_t heap_unit_size = __align_size(max_block_size);
    size_t heap_unit_count = heap_size / heap_unit_size;
    // heap unit count must be 2^n
    heap_unit_count = __fix_size(heap_unit_count);
    heap_size  = heap_unit_count * heap_unit_size;

    DBG("parameters after fixed: max_block_size<%lu>, chunk_size<%lu>, chunk_count<%d>, heap_size<%lu>, heap_unit_size<%lu>, heap_unit_count<%lu>.",
        max_block_size, chunk_size, chunk_count, heap_size, heap_unit_size, heap_unit_count);

    size_t singleton_size = 0;
    for (auto it = singleton_meta_info.begin(); it != singleton_meta_info.end(); ++it)
        singleton_size += it->size;

    size_t chunk_mgr_size = pool_mgr::calc_size(max_block_size);
    size_t heap_allocator_size = heap_allocator::calc_size(heap_unit_count);

    size_t shm_size = 0;
    shm_size += sizeof(shm_mgr);
    shm_size += singleton_size;
    shm_size += chunk_mgr_size;
    shm_size += heap_allocator_size;
    shm_size += chunk_size * chunk_count;
    shm_size += heap_size;

    DBG("shm size info: mgr<%lu>, singleton<%lu>, chunk mgr<%lu>, heap allocator<%lu>, chunk pool<%lu>, heap<%lu>, total<%lu>.",
        sizeof(shm_mgr), singleton_size, chunk_mgr_size, heap_allocator_size, chunk_size * chunk_count, heap_size, shm_size);

    sk::shm_seg seg;
    int ret = seg.init(key, shm_size, resume);
    if (ret != 0) {
        ERR("cannot create shm mgr, key<%d>, size<%lu>.", key, shm_size);
        return NULL;
    }

    shm_mgr *self = cast_ptr(shm_mgr, seg.address());
    char *base_addr = char_ptr(seg.address());
    char *chunk_pool = base_addr + singleton_size + chunk_mgr_size + heap_allocator_size;
    char *heap_pool = chunk_pool + chunk_size * chunk_count;
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

    // 1. init chunk_mgr
    {
        self->chunk_mgr = pool_mgr::create(base_addr, chunk_mgr_size, resume,
                                           chunk_pool, chunk_size * chunk_count,
                                           max_block_size, chunk_size, chunk_count);
        assert_retval(self->chunk_mgr, NULL);

        base_addr += chunk_mgr_size;
    }

    // 2. init heap allocator
    {
        self->heap = heap_allocator::create(base_addr, heap_allocator_size, resume,
                                            heap_pool, heap_size,
                                            heap_unit_count, heap_unit_size);
        assert_retval(self->heap, NULL);

        base_addr += heap_allocator_size;
    }

    if (resume) {
        assert_retval(self->shmid == seg.shmid, NULL);
        assert_retval(self->max_block_size == max_block_size, NULL);
    } else {
        self->shmid = seg.shmid;
        self->max_block_size = max_block_size;
    }

    seg.release();
    return self;
}

inline sk::shm_mgr *sk::shm_mgr::get() {
    assert_retval(mgr, NULL);
    return mgr;
}

int sk::shm_mgr::__malloc_from_chunk_pool(size_t mem_size, int& chunk_index, int& block_index) {
    return chunk_mgr->malloc(mem_size, chunk_index, block_index);
}

int sk::shm_mgr::__malloc_from_heap(size_t mem_size, int& unit_index) {
    return heap->malloc(mem_size, unit_index);
}

void sk::shm_mgr::__free_from_chunk_pool(int chunk_index, int block_index) {
    chunk_mgr->free(chunk_index, block_index);
}

void sk::shm_mgr::__free_from_heap(int unit_index) {
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

        return chunk_mgr->index2ptr(chunk_index, block_index);
    }
    case detail::PTR_TYPE_HEAP: {
        int unit_index = ptr->idx1;
        assert_retval(unit_index >= 0, NULL);

        return heap->index2ptr(unit_index);
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
