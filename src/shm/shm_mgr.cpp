#include <shm/shm_mgr.h>
#include <utility/config.h>
#include <utility/math_helper.h>
#include <shm/detail/size_map.h>
#include <shm/detail/block_mgr.h>
#include <shm/detail/page_heap.h>
#include <shm/detail/chunk_cache.h>
#include <shm/detail/shm_segment.h>

using namespace sk;
using namespace sk::detail;

static shm_mgr *mgr = nullptr;

struct shm_meta {
    size_t serial: shm_config::MAX_SERIAL_BITS;
    size_t magic:  sizeof(size_t) * CHAR_BIT - shm_config::MAX_SERIAL_BITS;
};
static_assert(sizeof(shm_meta) == sizeof(size_t), "invalid shm_meta");

int shm_mgr::init(const char *basename, bool resume_mode) {
    if (mgr) {
        sk_error("shm mgr already initialized.");
        return 0;
    }

    char path[shm_config::MAX_PATH_SIZE];
    snprintf(path, sizeof(path), "%s-mgr.mmap", basename);

    size_t total_bytes = 0;
    total_bytes += sizeof(shm_mgr);
    total_bytes += sizeof(detail::size_map);
    total_bytes += sizeof(detail::block_mgr);

    shm_segment seg;
    const size_t alignment = cast_size(getpagesize());
    int ret = seg.init(path, total_bytes, alignment, nullptr, resume_mode);
    if (ret != 0) {
        sk_error("cannot init shm: %s, ret: %d.", path, ret);
        return ret;
    }

    shm_mgr *x_mgr = cast_ptr(shm_mgr, seg.address());
    if (!resume_mode) {
        new (x_mgr) shm_mgr();
        ret = x_mgr->on_create(basename);
    } else {
        ret = x_mgr->on_resume(basename);
    }

    if (ret == 0) {
        seg.release();
        mgr = x_mgr;
    }

    return ret;
}

shm_ptr<void> shm_mgr::malloc(size_t bytes) {
    // this function will almost never fail, so we increase
    // this count at the beginning of this function
    ++mgr->stat_.alloc_count;

    // extra 8 bytes to store shm_meta struct
    bytes += sizeof(shm_meta);
    shm_address addr;

    do {
        u8 sc = 0;
        bool ok = mgr->size_map()->size2class(bytes, &sc);
        if (ok) {
            addr = mgr->chunk_cache()->allocate(bytes, sc);
            break;
        }

        size_t page_count = (bytes + shm_config::PAGE_MASK) >> shm_config::PAGE_SHIFT;
        shm_address sp = mgr->page_heap()->allocate_span(page_count);
        if (sp) {
            span *s = sp.as<span>();
            addr = shm_address(s->block, s->start_page << shm_config::PAGE_SHIFT);
        }
    } while (0);

    if (!addr) return nullptr;

    ++mgr->serial_;
    if (mgr->serial_ == shm_config::SPECIAL_SERIAL) ++mgr->serial_;
    if (mgr->serial_ >= shm_config::MAX_SERIAL_NUM) mgr->serial_ = 1;

    shm_meta *meta = addr.as<shm_meta>();
    meta->magic  = cast_u32(MAGIC);
    meta->serial = mgr->serial_;

    memset(sk::byte_offset<void>(meta, sizeof(shm_meta)), 0x00, bytes - sizeof(shm_meta));

    sk_trace("=> shm_mgr::malloc(): size<%lu>, serial<%lu>, block<%lu>, offset<%lu>, addr<%lu>.",
             bytes, mgr->serial_, addr.block(), addr.offset(), *cast_ptr(size_t, &addr));

    return shm_ptr<void>(addr);
}

void shm_mgr::free(const shm_ptr<void>& ptr) {
    assert_retnone(ptr);

    ++mgr->stat_.free_count;

    shm_address addr = ptr.address();
    assert_retnone(addr.offset() >= sizeof(shm_meta));

    shm_meta *meta = addr.as<shm_meta>();
    if (meta->magic != cast_u32(MAGIC) || meta->serial != addr.serial()) {
        sk_warn("invalid free, expected<serial: %lu>, actual<serial: %lu>, addr<%lu>.",
                addr.serial(), meta->serial, *cast_ptr(size_t, &addr));
        return;
    }

    meta->magic  = 0;
    meta->serial = 0;

    shm_address sp = mgr->page_heap()->find_span(addr);
    assert_retnone(sp);

    span *s = sp.as<span>();
    if (s->size_class < 0) {
        sk_assert(!s->chunk_list);
        sk_assert(s->used_count == 0);

        return mgr->page_heap()->deallocate_span(sp);
    }

    // TODO: the function below will search for span again, it's better to
    // take the searched span as a parameter to improve performance
    mgr->chunk_cache()->deallocate(addr);
}

bool shm_mgr::has_singleton(int type) {
    assert_retval(type >= 0 && type < MAX_SINGLETON_COUNT, false);
    return !!mgr->singletons_[type];
}

shm_ptr<void> shm_mgr::get_singleton(int type, size_t bytes, bool *first_call) {
    assert_retval(type >= 0 && type < MAX_SINGLETON_COUNT, nullptr);

    if (first_call) *first_call = false;
    if (!mgr->singletons_[type]) {
        shm_ptr<void> ptr = malloc(bytes);
        if (!ptr) return nullptr;

        if (first_call) *first_call = true;
        mgr->singletons_[type] = ptr;
    }

    return mgr->singletons_[type];
}

void shm_mgr::free_singleton(int type) {
    assert_retnone(type >= 0 && type < MAX_SINGLETON_COUNT);

    if (mgr->singletons_[type]) {
        free(mgr->singletons_[type]);
        mgr->singletons_[type] = nullptr;
    }
}

void *shm_mgr::addr2ptr(shm_address addr) {
    assert_retval(addr, nullptr);

    const shm_block *block = mgr->block_mgr()->get_block(addr.block());
    assert_retval(block, nullptr);
    assert_retval(block->size > addr.offset(), nullptr);

    if (addr.serial() == shm_config::SPECIAL_SERIAL)
        return sk::byte_offset<void>(block->addr, addr.offset());

    shm_meta *meta = sk::byte_offset<shm_meta>(block->addr, addr.offset());
    if (meta->magic != cast_u32(MAGIC) || meta->serial != addr.serial()) {
        sk_warn("object freed? expected<serial: %lu>, actual<serial: %lu>, addr<%lu>.",
                addr.serial(), meta->serial, *cast_ptr(size_t, &addr));
        return nullptr;
    }

    return sk::byte_offset<void>(meta, sizeof(shm_meta));
}

shm_address shm_mgr::ptr2addr(void *ptr) {
    assert_retval(ptr, nullptr);

    const shm_block *block = mgr->block_mgr()->find_block(ptr);
    if (!block) return nullptr;

    void *end = sk::byte_offset<void>(block->addr, block->size);
    assert_retval(ptr < end, nullptr);

    offset_t offset = reinterpret_cast<offset_t>(ptr) - reinterpret_cast<offset_t>(block->addr);
    return shm_address(block->id, offset);
}

detail::size_map *shm_mgr::size_map() {
    return mgr->size_map_;
}

detail::block_mgr *shm_mgr::block_mgr() {
    return mgr->block_mgr_;
}

detail::page_heap *shm_mgr::page_heap() {
    return mgr->page_heap_;
}

detail::chunk_cache *shm_mgr::chunk_cache() {
    return mgr->chunk_cache_;
}

shm_address shm_mgr::allocate(size_t in_bytes, size_t *out_bytes) {
    static bool calling = false;
    struct guard {
        guard() { sk_assert(!calling); calling = true; }
        ~guard() { calling = false; }
    } guard;

    const shm_block *block = mgr->block_mgr()->allocate_block(in_bytes);
    if (!block) return nullptr;

    // TODO: if the newly allocated block is adjacent to another existing block,
    // we might try to merge the two blocks, and further more, we might specify
    // the address after a block when allocating, if the returned address is the
    // address we specified, then we merge the two blocks, but there is a little
    // limitation we might check: due to MAX_PAGE_BITS defined in shm_config, we
    // can only have a block with maximum size 4GB

    sk_assert(block->size >= in_bytes);
    mgr->block_mgr()->register_block(block->id, false);

    if (out_bytes) *out_bytes = block->size;
    return shm_address(block->id);
}

shm_address shm_mgr::allocate_metadata(size_t bytes) {
    static bool calling = false;
    struct guard {
        guard() { sk_assert(!calling); calling = true; }
        ~guard() { calling = false; }
    } guard;

    if (unlikely(bytes == 0)) bytes = 1;

    if (unlikely(bytes % shm_config::PAGE_SIZE != 0)) {
        sk_info("fix metadata size: %lu.", bytes);
        bytes +=  shm_config::PAGE_SIZE - 1;
        bytes >>= shm_config::PAGE_SHIFT;
        bytes <<= shm_config::PAGE_SHIFT;
    }

    if (bytes > mgr->metadata_.current_meta_left) {
        sk_info("current metadata block %lu used up, waste: %lu.",
                mgr->metadata_.current_meta_block, mgr->metadata_.current_meta_left);

        // TODO: create a config value in shm_config, to represent the metadata
        // block allocation size
        const shm_block *block = mgr->block_mgr()->allocate_block(bytes);
        if (!block) return nullptr;

        mgr->metadata_.current_meta_block  = block->id;
        mgr->metadata_.current_meta_left   = block->size;
        mgr->metadata_.current_meta_offset = 0;
        assert_retval(block->size >= bytes, nullptr);

        // TODO: how to set_block_map() here? as in that function, radix_tree:set() is
        // called, which might call allocate_metadata() again if there is no enough
        // memory, this will lead to a recursive call, which might lead to bugs...
    }

    shm_address addr(mgr->metadata_.current_meta_block, mgr->metadata_.current_meta_offset);
    mgr->metadata_.current_meta_left   -= bytes;
    mgr->metadata_.current_meta_offset += bytes;
    return addr;
}

int shm_mgr::on_create(const char *basename) {
    int ret = 0;
    size_t used_bytes = 0;

    used_bytes += sizeof(*this);
    size_map_ = sk::byte_offset<detail::size_map>(this, used_bytes);
    ret = size_map_->init();
    if (ret != 0) return ret;

    used_bytes += sizeof(detail::size_map);
    block_mgr_ = sk::byte_offset<detail::block_mgr>(this, used_bytes);
    new (block_mgr_) detail::block_mgr(basename);

    // TODO: make this a config option in shm_config
    // TODO: change this to a small value to test new block allocation when
    // the following set_block_map() called, which will allocate a new block
    // if the existing metadata is not enough
    size_t total_bytes = 2 * 1024 * 1024;
    total_bytes += sizeof(detail::page_heap);
    total_bytes += sizeof(detail::chunk_cache);

    const shm_block *block = block_mgr()->allocate_block(total_bytes);
    if (!block) return -ENOMEM;

    used_bytes = 0;

    page_heap_ = sk::byte_offset<detail::page_heap>(block->addr, used_bytes);
    used_bytes += sizeof(detail::page_heap);

    chunk_cache_ = sk::byte_offset<detail::chunk_cache>(block->addr, used_bytes);
    used_bytes += sizeof(detail::chunk_cache);

    new (page_heap_) detail::page_heap();
    new (chunk_cache_) detail::chunk_cache();

    // make metadata page-aligned, as allocate_metadata()
    // will forcely align the requested size to page size
    used_bytes += shm_config::PAGE_SIZE - (used_bytes & shm_config::PAGE_MASK);
    sk_assert((used_bytes & shm_config::PAGE_MASK) == 0);

    metadata_.current_meta_block = block->id;
    metadata_.current_meta_offset = used_bytes;
    metadata_.current_meta_left = block->size > used_bytes ? block->size - used_bytes : 0;

    block_mgr()->register_block(block->id, false);
    special_block_ = block->id;
    return 0;
}

int shm_mgr::on_resume(const char *basename) {
    int ret = 0;
    size_t used_bytes = 0;

    used_bytes += sizeof(*this);
    size_map_ = sk::byte_offset<detail::size_map>(this, used_bytes);

    used_bytes += sizeof(detail::size_map);
    block_mgr_ = sk::byte_offset<detail::block_mgr>(this, used_bytes);

    std::vector<block_t> changed_blocks;
    ret = block_mgr_->on_resume(basename, changed_blocks);
    if (ret != 0) return ret;

    const shm_block *block = block_mgr()->get_block(special_block_);
    if (!block) return -EINVAL;

    used_bytes = 0;
    page_heap_ = sk::byte_offset<detail::page_heap>(block->addr, used_bytes);

    used_bytes += sizeof(detail::page_heap);
    chunk_cache_ = sk::byte_offset<detail::chunk_cache>(block->addr, used_bytes);

    for (const auto& id : changed_blocks)
        block_mgr()->register_block(id, true);

    return 0;
}

shm_ptr<void> shm_malloc(size_t bytes) {
    return shm_mgr::malloc(bytes);
}

void sk::shm_free(const shm_ptr<void>& ptr) {
    return mgr->free(ptr);
}

int shm_mgr_init(const char *basename, bool resume_mode) {
    return shm_mgr::init(basename, resume_mode);
}

int shm_mgr_fini() {
    if (!mgr) return 0;

    // TODO: call desctructors and destroy shm segments here
    return 0;
}
