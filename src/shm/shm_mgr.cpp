#include "libsk.h"
#include "shm/shm_mgr.h"
#include "shm/detail/span.h"
#include "shm/detail/size_map.h"
#include "shm/detail/chunk_cache.h"
#include "shm/detail/page_heap.h"
#include "shm/detail/shm_segment.h"
#include "utility/config.h"

static sk::shm_mgr *mgr = NULL;

namespace sk {

int shm_mgr_init(key_t key, size_t size_hint, bool resume) {
    mgr = shm_mgr::create(key, size_hint, resume);
    if (!mgr)
        return -EINVAL;

    return mgr->init(resume);
}

int shm_mgr_fini() {
    if (!mgr)
        return 0;

    // TODO: we may need to call destructors of the objects
    // still in pool here
    shmctl(mgr->shmid, IPC_RMID, 0);

    mgr = NULL;
    return 0;
}

int shm_mgr::init(bool resume) {
    assert_retval(shmid != 0, -1);
    assert_retval(used_size != 0, -1);
    assert_retval(total_size != 0, -1);

    char *base_addr = char_ptr(this);
    base_addr += sizeof(*this);

    // 0. init size map
    {
        size_map = cast_ptr(detail::size_map, base_addr);
        base_addr += sizeof(detail::size_map);
        if (!resume) {
            int ret = size_map->init();
            if (ret != 0) {
                ERR("size map init failure: %d.", ret);
                return ret;
            }
        }
    }

    // 1. init chunk cache
    {
        chunk_cache = cast_ptr(detail::chunk_cache, base_addr);
        base_addr += sizeof(detail::chunk_cache);
        if (!resume)
            chunk_cache->init();
    }

    // 2. init page heap
    {
        page_heap = cast_ptr(detail::page_heap, base_addr);
        base_addr += sizeof(detail::page_heap);
        if (!resume)
            page_heap->init();
    }

    // 3. init metadata block
    {
        if (!resume) {
            metadata_offset = base_addr - char_ptr(this);
            metadata_left = used_size - metadata_offset;
        }
        base_addr += metadata_left;
    }

    return 0;
}

shm_mgr *shm_mgr::create(key_t key, size_t size_hint, bool resume) {
    if (size_hint < MIN_SHM_SPACE)
        size_hint = MIN_SHM_SPACE;

    INF("shm mgr creation, size_hint: %lu.", size_hint);

    size_t metadata_size = detail::page_heap::estimate_space(size_hint);
    // the calculated size is the maximum possible size, so
    // we multiple a factor to get the final size
    metadata_size = static_cast<size_t>(metadata_size * 0.5);
    if (metadata_size < MIN_METADATA_SPACE)
        metadata_size = MIN_METADATA_SPACE;

    size_t shm_size = 0;
    shm_size += sizeof(shm_mgr);
    shm_size += sizeof(detail::size_map);
    shm_size += sizeof(detail::chunk_cache);
    shm_size += sizeof(detail::page_heap);
    shm_size += metadata_size;
    shm_size += size_hint;

    detail::shm_segment seg;
    int ret = seg.init(key, shm_size, resume);
    if (ret != 0) {
        ERR("cannot create shm_mgr, key<%d>, size<%lu>.", key, shm_size);
        return NULL;
    }

    char *base_addr = char_ptr(seg.address());
    shm_mgr *self = cast_ptr(shm_mgr, base_addr);

    if (!resume) {
        memset(self, 0x00, sizeof(*self));

        self->shmid = seg.shmid;
        self->total_size = shm_size;

        self->used_size = 0;
        self->used_size += sizeof(*self);
        self->used_size += sizeof(detail::size_map);
        self->used_size += sizeof(detail::chunk_cache);
        self->used_size += sizeof(page_heap);
        self->used_size += metadata_size;

        // do page alignment
        offset_t offset = ((self->used_size + PAGE_SIZE - 1) >> PAGE_SHIFT) << PAGE_SHIFT;
        assert_retval(self->used_size <= offset, NULL);
        self->used_size = offset;
    } else {
        assert_retval(self->shmid == seg.shmid, NULL);
        assert_retval(self->total_size == shm_size, NULL);
    }

    seg.release();
    return self;
}

shm_mgr *shm_mgr::get() {
    assert_retval(mgr, NULL);
    return mgr;
}

shm_ptr<void> shm_mgr::malloc(size_t bytes) {
    if (bytes <= MAX_SIZE)
        return chunk_cache->allocate(bytes);

    int page_count = (bytes >> PAGE_SHIFT) + ((bytes & (PAGE_SIZE - 1)) > 0 ? 1 : 0);
    shm_ptr<detail::span> sp = page_heap->allocate_span(page_count);
    if (!sp)
        return SHM_NULL;

    offset_t offset = sp->start << PAGE_SHIFT;
    return shm_ptr<void>(offset);
}

void shm_mgr::free(shm_ptr<void> ptr) {
    assert_retnone(ptr);

    page_t page = ptr2page(ptr);
    shm_ptr<detail::span> sp = page_heap->find_span(page);
    assert_retnone(sp);

    if (sp->size_class < 0) {
        assert_noeffect(!sp->chunk_list);
        assert_noeffect(sp->used_count == 0);

        return page_heap->deallocate_span(sp);
    }

    // TODO: the function below will search for span again, it's better to
    // take the searched span as a parameter to improve performance
    chunk_cache->deallocate(ptr);
}

offset_t shm_mgr::__sbrk(size_t bytes) {
    // no enough memory, return
    if (used_size + bytes > total_size) {
        ERR("no enough memory, used: %lu, total: %lu, needed: %lu.", used_size, total_size, bytes);
        return OFFSET_NULL;
    }

    offset_t offset = used_size;
    used_size += bytes;

    return offset;
}

offset_t shm_mgr::allocate(size_t bytes) {
    if (bytes % PAGE_SIZE != 0) {
        DBG("bytes: %lu to be fixed.", bytes);
        bytes = ((bytes + PAGE_SIZE - 1) >> PAGE_SHIFT) << PAGE_SHIFT;
    }

    return __sbrk(bytes);
}

offset_t shm_mgr::allocate_metadata(size_t bytes) {
    if (bytes % PAGE_SIZE != 0) {
        DBG("bytes: %lu to be fixed.", bytes);
        bytes = ((bytes + PAGE_SIZE - 1) >> PAGE_SHIFT) << PAGE_SHIFT;
    }

    if (bytes > metadata_left) {
        DBG("metadata block used up.");
        return allocate(bytes);
    }

    offset_t offset = metadata_offset;
    metadata_offset += bytes;
    metadata_left -= bytes;

    return offset;
}

void *shm_mgr::offset2ptr(offset_t offset) {
    assert_retval(offset >= sizeof(*this), NULL);
    assert_retval(offset < used_size, NULL);

    return char_ptr(this) + offset;
}

offset_t shm_mgr::ptr2offset(void *ptr) {
    char *p = char_ptr(ptr);
    char *base = char_ptr(this);
    assert_retval(p >= base + sizeof(this), OFFSET_NULL);
    assert_retval(p < base + used_size, OFFSET_NULL);

    return p - base;
}

page_t shm_mgr::ptr2page(shm_ptr<void> ptr) {
    assert_noeffect(ptr.offset >= sizeof(*this));
    assert_noeffect(ptr.offset < used_size);

    return ptr.offset >> PAGE_SHIFT;
}



shm_ptr<void> shm_malloc(size_t bytes) {
    shm_mgr *mgr = shm_mgr::get();
    assert_retval(mgr, SHM_NULL);

    return mgr->malloc(bytes);
}

void shm_free(shm_ptr<void> ptr) {
    shm_mgr *mgr = shm_mgr::get();
    assert_retnone(mgr);

    mgr->free(ptr);
}

} // namespace sk

