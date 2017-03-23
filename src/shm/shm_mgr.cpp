#include "libsk.h"
#include "shm/shm_mgr.h"
#include "shm/detail/span.h"
#include "shm/detail/size_map.h"
#include "shm/detail/chunk_cache.h"
#include "shm/detail/page_heap.h"
#include "shm/detail/shm_segment.h"
#include "utility/config.h"
#include "utility/math_helper.h"

static sk::shm_mgr *mgr = NULL;

static const size_t SERIAL_BIT_COUNT = 28;
static const size_t OFFSET_BIT_COUNT = 64 - SERIAL_BIT_COUNT;
static_assert(OFFSET_BIT_COUNT >= MAX_MEM_SHIFT, "offset not enough");

struct shm_mid {
    u64 offset: OFFSET_BIT_COUNT;
    u64 serial: SERIAL_BIT_COUNT;
};
static_assert(sizeof(shm_mid) == sizeof(u64), "shm_mid must be sizeof(u64)");

struct shm_meta {
    u64 magic:  OFFSET_BIT_COUNT;
    u64 serial: SERIAL_BIT_COUNT;
};
static_assert(sizeof(shm_meta) == sizeof(u64), "shm_meta must be sizeof(u64)");

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
                sk_error("size map init failure: %d.", ret);
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

void shm_mgr::report() {
    sk_info("=============================================================================");
    chunk_cache->report();
    page_heap->report();

    sk_info("shm mgr => total: %lu, used: %lu (%.2lf%%), meta left: %lu.",
            total_size, used_size, (used_size * 100.0 / total_size), metadata_left);
    sk_info("shm mgr => allocation count: %lu, deallocation count: %lu.",
            stat.alloc_count, stat.free_count);
    sk_info("shm mgr => raw memory allocation count: %lu, meta data allocation count: %lu.",
            stat.raw_alloc_count, stat.meta_alloc_count);
    sk_info("=============================================================================");
}

shm_mgr *shm_mgr::create(key_t key, size_t size_hint, bool resume) {
    if (size_hint < MIN_SHM_SPACE)
        size_hint = MIN_SHM_SPACE;

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

    sk_info("shm mgr creation, size_hint: %lu, fixed size: %lu.", size_hint, shm_size);

    detail::shm_segment seg;
    int ret = seg.init(key, shm_size, resume);
    if (ret != 0) {
        sk_error("cannot create shm_mgr, key<%d>, size<%lu>.", key, shm_size);
        return NULL;
    }

    shm_mgr *self = cast_ptr(shm_mgr, seg.address());

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
    // this function will almost never fail, so we increase
    // this count at the beginning of this function
    ++stat.alloc_count;

    // extra 8 bytes to store shm_meta struct
    bytes += sizeof(shm_meta);

    detail::offset_ptr<void> ptr;
    do {
        if (bytes <= MAX_SIZE) {
            ptr = chunk_cache->allocate(bytes);
            break;
        }

        int page_count = (bytes >> PAGE_SHIFT) + ((bytes & (PAGE_SIZE - 1)) > 0 ? 1 : 0);
        detail::offset_ptr<detail::span> sp = page_heap->allocate_span(page_count);
        if (sp) {
            detail::span *s = sp.get();
            ptr = detail::offset_ptr<void>(s->start << PAGE_SHIFT);
            break;
        }
    } while (0);

    if (!ptr)
        return SHM_NULL;

    ++serial;
    if (serial >= (1ULL << SERIAL_BIT_COUNT))
        serial = 1;

    u64 mid = MID_NULL;
    shm_mid *pmid = cast_ptr(shm_mid, &mid);
    pmid->offset = ptr.offset;
    pmid->serial = serial;

    shm_meta *pmeta = ptr.as<shm_meta>().get();
    pmeta->magic  = static_cast<u32>(MAGIC);
    pmeta->serial = serial;

    do {
        char *p = sk::byte_offset<char>(pmeta, sizeof(*pmeta));
        size_t s = bytes - sizeof(shm_meta);
        memset(p, 0x00, s);
    } while (0);

    sk_trace("=> shm_mgr::malloc(): size<%lu>, serial<%lu>, offset<%lu>, mid<%lu>.",
             bytes, serial, ptr.offset, mid);

#ifdef NDEBUG
#define COUNT 10000
#else
#define COUNT 100
#endif

    if (stat.alloc_count % COUNT == 0)
        report();

#undef COUNT

    return shm_ptr<void>(mid);
}

void shm_mgr::free(shm_ptr<void> ptr) {
    assert_retnone(ptr);

    ++stat.free_count;

    u64 mid = ptr.mid;
    const shm_mid *pmid = cast_ptr(shm_mid, &mid);
    assert_retnone(pmid->offset != OFFSET_NULL);

    detail::offset_ptr<void> offset(pmid->offset);
    shm_meta *pmeta = offset.as<shm_meta>().get();
    if (pmeta->magic != static_cast<u32>(MAGIC) || pmeta->serial != pmid->serial) {
        sk_warn("invalid free, expected<serial: %lu>, actual<serial: %lu>.",
                pmid->serial, pmeta->serial);
        return;
    }

    pmeta->magic = 0;
    pmeta->serial = 0;

    page_t page = offset2page(pmid->offset);
    detail::offset_ptr<detail::span> sp = page_heap->find_span(page);
    assert_retnone(sp);

    detail::span *s = sp.get();
    if (s->size_class < 0) {
        sk_assert(!s->chunk_list);
        sk_assert(s->used_count == 0);

        return page_heap->deallocate_span(sp);
    }

    // TODO: the function below will search for span again, it's better to
    // take the searched span as a parameter to improve performance
    chunk_cache->deallocate(offset);
}

shm_ptr<void> shm_mgr::get_singleton(int type, size_t bytes, bool& first_call) {
    assert_retval(type >= 0 && type < MAX_SINGLETON_COUNT, SHM_NULL);

    first_call = false;
    if (singletons[type] == MID_NULL) {
        shm_ptr<void> ptr = malloc(bytes);
        if (!ptr)
            return SHM_NULL;

        first_call = true;
        singletons[type] = ptr.mid;
    }

    return shm_ptr<void>(singletons[type]);
}

offset_t shm_mgr::__sbrk(size_t bytes) {
    // no enough memory, return
    if (used_size + bytes > total_size) {
        sk_error("no enough memory, used: %lu, total: %lu, needed: %lu.", used_size, total_size, bytes);
        return OFFSET_NULL;
    }

    offset_t offset = used_size;
    used_size += bytes;

    return offset;
}

offset_t shm_mgr::allocate(size_t bytes) {
    if (bytes % PAGE_SIZE != 0) {
        sk_info("bytes: %lu to be fixed.", bytes);
        bytes = ((bytes + PAGE_SIZE - 1) >> PAGE_SHIFT) << PAGE_SHIFT;
    }

    ++stat.raw_alloc_count;

    return __sbrk(bytes);
}

offset_t shm_mgr::allocate_metadata(size_t bytes) {
    if (bytes % PAGE_SIZE != 0) {
        sk_info("bytes: %lu to be fixed.", bytes);
        bytes = ((bytes + PAGE_SIZE - 1) >> PAGE_SHIFT) << PAGE_SHIFT;
    }

    if (bytes > metadata_left) {
        sk_info("metadata block used up.");
        return allocate(bytes);
    }

    offset_t offset = metadata_offset;
    metadata_offset += bytes;
    metadata_left -= bytes;

    ++stat.meta_alloc_count;

    return offset;
}

page_t shm_mgr::offset2page(offset_t offset) {
    sk_assert(offset >= sizeof(*this));
    sk_assert(offset < used_size);

    return offset >> PAGE_SHIFT;
}

void *shm_mgr::offset2ptr(offset_t offset) {
    assert_retval(offset >= sizeof(*this), NULL);
    assert_retval(offset < used_size, NULL);

    return sk::byte_offset<void>(this, offset);
}

offset_t shm_mgr::ptr2offset(void *ptr) {
    char *p = char_ptr(ptr);
    char *base = char_ptr(this);
    assert_retval(p >= base + sizeof(*this), OFFSET_NULL);
    assert_retval(p < base + used_size, OFFSET_NULL);

    return p - base;
}

void *shm_mgr::mid2ptr(u64 mid) {
    const shm_mid *pmid = cast_ptr(shm_mid, &mid);
    assert_retval(pmid->offset != OFFSET_NULL, NULL);

    void *ptr = offset2ptr(pmid->offset);
    const shm_meta *pmeta = cast_ptr(shm_meta, ptr);

    if (pmeta->magic != static_cast<u32>(MAGIC) || pmeta->serial != pmid->serial) {
        sk_warn("object freed? expected<serial: %lu>, actual<serial: %lu>.",
                pmid->serial, pmeta->serial);
        return NULL;
    }

    return sk::byte_offset<void>(ptr, sizeof(shm_meta));
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

