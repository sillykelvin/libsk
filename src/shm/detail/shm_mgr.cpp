#include <sys/sysinfo.h>
#include <shm/shm_config.h>
#include <shm/detail/shm_mgr.h>
#include <shm/detail/size_map.h>
#include <shm/detail/page_heap.h>
#include <shm/detail/shm_object.h>
#include <utility/assert_helper.h>
#include <shm/detail/chunk_cache.h>

using namespace sk;
using namespace sk::detail;

struct shm_meta {
    u64 serial : shm_config::SERIAL_BITS;
    u64 magic  : sizeof(u64) * CHAR_BIT - shm_config::SERIAL_BITS;
};
static_assert(sizeof(shm_meta) == sizeof(size_t), "invalid shm_meta");

shm_mgr::shm_mgr()
    : serial_(0), default_mmap_size_(0),
      size_map_(nullptr), page_heap_(nullptr), chunk_cache_(nullptr) {
    memset(basename_, 0x00, sizeof(basename_));
}

shm_mgr::~shm_mgr() {
    // TODO: desctruction here
}

int shm_mgr::on_create(const char *basename) {
    struct sysinfo info;
    sk_assert(sysinfo(&info) == 0);

    serial_ = shm_config::MIN_VALID_SERIAL_NUM;
    default_mmap_size_ = info.totalram & ~shm_config::PAGE_MASK;
    snprintf(basename_, sizeof(basename_), "%s", basename);

    size_t real_size = 0;
    real_size += sizeof(class size_map);
    real_size += sizeof(class page_heap);
    real_size += sizeof(class chunk_cache);
    real_size += shm_config::PAGE_SIZE;
    // TODO: change this to a small value to test new block allocation when
    // the following set_block_map() called, which will allocate a new block
    // if the existing metadata is not enough
    // real_size += 2 * 1024 * 1024; // TODO: make this an option

    size_t mmap_size = default_mmap_size_;
    shm_block *block = &blocks_[METADATA_BLOCK];
    sk_assert(!block->addr);

    int ret = create_block(METADATA_BLOCK, real_size, mmap_size);
    if (ret != 0) return ret;

    size_map_ = sk::byte_offset<class size_map>(block->addr, block->used_size);
    block->used_size += sizeof(class size_map);
    ret = size_map_->init();
    if (ret != 0) {
        unlink_block(METADATA_BLOCK);
        return ret;
    }

    page_heap_ = sk::byte_offset<class page_heap>(block->addr, block->used_size);
    block->used_size += sizeof(class page_heap);
    new (page_heap_) class page_heap();

    chunk_cache_ = sk::byte_offset<class chunk_cache>(block->addr, block->used_size);
    block->used_size += sizeof(class chunk_cache);
    new (chunk_cache_) class chunk_cache();

    // make metadata page-aligned, as allocate_metadata()
    // will forcely align the requested size to page size
    block->used_size += shm_config::PAGE_SIZE - (block->used_size & shm_config::PAGE_MASK);
    sk_assert((block->used_size & shm_config::PAGE_MASK) == 0);
    sk_assert(block->used_size <= block->real_size);

    return 0;
}

int shm_mgr::on_resume(const char *basename) {
    assert_retval(strcmp(basename_, basename) == 0, -EINVAL);

    int ret = 0;
    size_t used_size = 0;

    ret = attach_block(METADATA_BLOCK);
    if (ret != 0) return ret;

    ret = attach_block(USERDATA_BLOCK);
    if (ret != 0) return ret;

    shm_block *block = &blocks_[METADATA_BLOCK];

    size_map_ = sk::byte_offset<class size_map>(block->addr, used_size);
    used_size += sizeof(class size_map);

    page_heap_ = sk::byte_offset<class page_heap>(block->addr, used_size);
    used_size += sizeof(class page_heap);

    chunk_cache_ = sk::byte_offset<class chunk_cache>(block->addr, used_size);
    used_size += sizeof(class chunk_cache);

    return 0;
}

shm_ptr<void> shm_mgr::malloc(size_t bytes) {
    // this function will almost never fail, so we increase
    // this count at the beginning of this function
    ++stat_.alloc_count;

    // extra 8 bytes to store shm_meta struct
    bytes += sizeof(shm_meta);
    shm_address addr;

    do {
        u8 sc = 0;
        bool ok = size_map_->size2class(bytes, &sc);
        if (ok) {
            addr = chunk_cache_->allocate_chunk(bytes, sc);
            break;
        }

        size_t page_count = (bytes + shm_config::PAGE_MASK) >> shm_config::PAGE_BITS;
        shm_address sp = page_heap_->allocate_span(page_count);
        if (sp) {
            span *s = sp.as<span>();
            shm_offset_t offset = s->start_page() << shm_config::PAGE_BITS;
            addr = shm_address(shm_config::USERDATA_SERIAL_NUM, offset);
        }
    } while (0);

    if (!addr) return nullptr;

    shm_meta *meta = addr.as<shm_meta>();
    meta->magic  = SK_MAGIC;
    meta->serial = serial_++;

    if ((serial_ & shm_config::SERIAL_MASK) == 0)
        serial_ = shm_config::MIN_VALID_SERIAL_NUM;

    addr = shm_address(meta->serial, addr.offset() + sizeof(shm_meta));

    sk_trace("=> shm_mgr::malloc(): size<%lu>, serial<%lu>, offset<%lu>, mid<%lu>.",
             bytes, meta->serial, addr.offset(), addr.as_u64());

    memset(sk::byte_offset<void>(meta, sizeof(shm_meta)), 0x00, bytes - sizeof(shm_meta));
    return shm_ptr<void>(addr);
}

void shm_mgr::free(const shm_ptr<void>& ptr) {
    assert_retnone(ptr);

    ++stat_.free_count;

    shm_address addr = ptr.address();
    assert_retnone(addr.serial() >= shm_config::MIN_VALID_SERIAL_NUM);
    assert_retnone(addr.offset() >= sizeof(shm_meta));

    shm_address base(shm_config::USERDATA_SERIAL_NUM, addr.offset() - sizeof(shm_meta));
    shm_meta *meta = base.as<shm_meta>();
    if (meta->magic != SK_MAGIC || meta->serial != addr.serial()) {
        sk_warn("invalid free, expected<serial: %lu>, actual<serial: %lu>, addr<%lu>.",
                addr.serial(), meta->serial, addr.as_u64());
        return;
    }

    meta->magic  = 0;
    meta->serial = 0;

    shm_address sp = page_heap_->find_span(base);
    assert_retnone(sp);

    span *s = sp.as<span>();
    if (s->size_class() < 0) {
        sk_assert(!s->chunk_list());
        sk_assert(s->used_count() == 0);

        return page_heap_->deallocate_span(sp);
    }

    chunk_cache_->deallocate_chunk(base, sp);
}

bool shm_mgr::has_singleton(int id) {
    assert_retval(id >= 0 && id < MAX_SINGLETON_COUNT, false);
    return !!singletons_[id];
}

shm_ptr<void> shm_mgr::get_singleton(int id, size_t bytes, bool *first_call) {
    assert_retval(id >= 0 && id < MAX_SINGLETON_COUNT, nullptr);

    if (first_call) *first_call = false;
    if (!singletons_[id]) {
        shm_ptr<void> ptr = malloc(bytes);
        if (!ptr) return nullptr;

        if (first_call) *first_call = true;
        singletons_[id] = ptr;
    }

    return singletons_[id];
}

void shm_mgr::free_singleton(int id) {
    assert_retnone(id >= 0 && id < MAX_SINGLETON_COUNT);

    if (singletons_[id]) {
        free(singletons_[id]);
        singletons_[id] = nullptr;
    }
}

shm_address shm_mgr::allocate_metadata(size_t *bytes) {
    ++stat_.metadata_alloc_count;
    return sbrk(METADATA_BLOCK, bytes);
}

shm_address shm_mgr::allocate_userdata(size_t *bytes) {
    ++stat_.userdata_alloc_count;
    return sbrk(USERDATA_BLOCK, bytes);
}

void *shm_mgr::addr2ptr(const shm_address& addr) {
    assert_retval(addr, nullptr);

    if (addr.serial() < shm_config::MIN_VALID_SERIAL_NUM) {
        assert_retval(addr.serial() == shm_config::METADATA_SERIAL_NUM ||
                      addr.serial() == shm_config::USERDATA_SERIAL_NUM, nullptr);

        shm_block *block = &blocks_[addr.serial() - 1];
        assert_retval(addr.offset() < block->used_size, nullptr);

        return sk::byte_offset<void>(block->addr, addr.offset());
    }

    shm_block *block = &blocks_[USERDATA_BLOCK];
    assert_retval(addr.offset() < block->used_size, nullptr);
    assert_retval(addr.offset() >= sizeof(shm_meta), nullptr);

    shm_meta *meta = sk::byte_offset<shm_meta>(block->addr, addr.offset() - sizeof(shm_meta));
    if (meta->magic != SK_MAGIC || meta->serial != addr.serial()) {
        sk_warn("object freed? expected<serial: %lu>, actual<serial: %lu>, addr<%lu>.",
                addr.serial(), meta->serial, addr.as_u64());
        return nullptr;
    }

    return sk::byte_offset<void>(meta, sizeof(shm_meta));
}

shm_address shm_mgr::ptr2addr(const void *ptr) {
    assert_retval(ptr, nullptr);

    static const shm_serial_t serials[] = {
        shm_config::METADATA_SERIAL_NUM, shm_config::USERDATA_SERIAL_NUM
    };
    static_assert(array_len(blocks_) == array_len(serials), "array size mismatch");

    for (size_t i = 0; i < array_len(blocks_); ++i) {
        shm_block *block = &blocks_[i];
        check_continue(block->addr && ptr >= block->addr);

        void *end = sk::byte_offset<void>(block->addr, block->used_size);
        check_continue(ptr < end);

        shm_offset_t offset = shm_offset_t(ptr) - shm_offset_t(block->addr);
        return shm_address(serials[i], offset);
    }

    sk_assert(0);
    return nullptr;
}

shm_address shm_mgr::sbrk(int block_index, size_t *bytes) {
    size_t real_bytes = *bytes;
    if ((real_bytes & shm_config::PAGE_MASK) != 0) {
        sk_warn("bytes<%lu> is not aligned<%lu>.", real_bytes, shm_config::PAGE_SIZE);
        real_bytes += shm_config::PAGE_SIZE - (real_bytes & shm_config::PAGE_MASK);
        sk_assert((real_bytes & shm_config::PAGE_MASK) == 0);
    }

    static const shm_serial_t serials[] = {
        shm_config::METADATA_SERIAL_NUM, shm_config::USERDATA_SERIAL_NUM
    };
    static_assert(array_len(blocks_) == array_len(serials), "array size mismatch");

    static const size_t grow_size[] = {
        shm_config::METADATA_GROW_SIZE, shm_config::USERDATA_GROW_SIZE
    };
    static_assert(array_len(blocks_) == array_len(grow_size), "array size mismatch");

    shm_block *block = &blocks_[block_index];
    do {
        if (block->addr) {
            sk_assert(block->used_size <= block->real_size);
            check_break(block->real_size - block->used_size < real_bytes);

            char path[shm_config::MAX_PATH_SIZE];
            calc_path(block_index, path, sizeof(path));

            size_t new_real_size = block->real_size;
            do {
                new_real_size += grow_size[block_index];
            } while (new_real_size - block->used_size < real_bytes);

            int ret = resize_block(block_index, new_real_size);
            check_break(ret != 0);

            return nullptr;
        }

        size_t mmap_size = default_mmap_size_;
        size_t real_size = grow_size[block_index];
        if (real_size < real_bytes) real_size = real_bytes;

        int ret = create_block(block_index, real_size, mmap_size);
        if (ret != 0) return nullptr;
    } while (0);

    sk_assert(block->addr && block->real_size - block->used_size >= real_bytes);

    shm_address addr(serials[block_index], block->used_size);
    block->used_size += real_bytes;
    *bytes = real_bytes;
    return addr;
}

int shm_mgr::create_block(int block_index, size_t real_size, size_t mmap_size) {
    int shmfd = -1;
    void *addr = nullptr;
    char path[shm_config::MAX_PATH_SIZE];

    do {
        calc_path(block_index, path, sizeof(path));
        shmfd = shm_object_create(path, &real_size);
        check_break(shmfd != -1);

        addr = shm_object_map(shmfd, &mmap_size, shm_config::PAGE_SIZE);
        check_break(addr);

        shm_block *block = &blocks_[block_index];
        block->addr = addr;
        block->used_size = 0;
        block->real_size = real_size;
        block->mmap_size = mmap_size;

        close(shmfd);
        return 0;
    } while (0);

    if (addr) shm_object_unmap(addr, mmap_size);
    if (shmfd != -1) {
        close(shmfd);
        shm_object_unlink(path);
    }

    return -1;
}

int shm_mgr::resize_block(int block_index, size_t new_real_size) {
    int shmfd = -1;
    char path[shm_config::MAX_PATH_SIZE];
    shm_block *block = &blocks_[block_index];

    if (new_real_size > block->mmap_size) {
        sk_error("real size<%lu> is too large, block<%d>, mmap size<%lu>.",
                 new_real_size, block_index, block->mmap_size);
        return -1;
    }

    do {
        calc_path(block_index, path, sizeof(path));
        shmfd = shm_object_resize(path, &new_real_size);
        check_break(shmfd != -1);

        block->real_size = new_real_size;

        close(shmfd);
        return 0;
    } while (0);

    if (shmfd != -1) close(shmfd);
    return -1;
}

int shm_mgr::attach_block(int block_index) {
    int shmfd = -1;
    size_t real_size = 0;
    size_t mmap_size = 0;
    void *addr = nullptr;
    char path[shm_config::MAX_PATH_SIZE];
    shm_block *block = &blocks_[block_index];

    // the block is not initialized, do nothing
    check_retval(block->addr, 0);

    do {
        calc_path(block_index, path, sizeof(path));
        shmfd = shm_object_attach(path, &real_size);
        check_break(shmfd != -1);
        assert_retval(real_size == block->real_size, -EINVAL);

        mmap_size = block->mmap_size;
        addr = shm_object_map(shmfd, &mmap_size, shm_config::PAGE_SIZE);
        check_break(addr);
        assert_retval(mmap_size == block->mmap_size, -EINVAL);

        block->addr = addr;
        close(shmfd);
        return 0;
    } while (0);

    if (addr) shm_object_unmap(addr, mmap_size);
    if (shmfd != -1) close(shmfd);

    return -1;
}

int shm_mgr::unlink_block(int block_index) {
    shm_block *block = &blocks_[block_index];
    if (block->addr) {
        shm_object_unmap(block->addr, block->mmap_size);
        block->addr = nullptr;
        block->used_size = 0;
        block->real_size = 0;
        block->mmap_size = 0;

        char path[shm_config::MAX_PATH_SIZE];
        calc_path(block_index, path, sizeof(path));
        shm_object_unlink(path);
    }

    return 0;
}
