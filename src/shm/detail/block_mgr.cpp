#include <shm/detail/block_mgr.h>
#include <shm/detail/shm_segment.h>

using namespace sk::detail;

int block_mgr::on_resume(const char *basename, std::vector<block_t>& changed_blocks) {
    assert_retval(strcmp(basename_, basename) == 0, -EINVAL);

    char path[shm_config::MAX_PATH_SIZE];
    for (block_t i = 0; i < array_len(blocks_); ++i) {
        check_continue(!block_slots_.test(i));

        shm_block& block = blocks_[i];
        sk_assert(block.addr);
        sk_assert(block.id == i);
        sk_assert(block.size > 0);
        sk_assert((block.size & (shm_config::ALIGNMENT - 1)) == 0);

        // TODO: test: do not attach to origin address and test the following set_block_map()
        shm_segment seg;
        calc_path(i, path, sizeof(path));
        int ret = seg.init(path, block.size, shm_config::ALIGNMENT, block.addr, true);
        if (ret != 0) {
            sk_error("cannot init shm: %s, ret: %d.", path, ret);
            return ret;
        }

        sk_assert(seg.size() == block.size);

        if (seg.address() != block.addr) {
            sk_info("block addr change: %p -> %p.", block.addr, seg.address());
            if (block.mapping) changed_blocks.push_back(i);
        }

        block.addr = seg.address();
        seg.release();
    }

    return 0;
}

const shm_block *block_mgr::allocate_block(size_t bytes) {
    if (bytes > shm_config::MAX_HEAP_GROW_SIZE) {
        sk_error("size too large: %lu.", bytes);
        return nullptr;
    }

    if (bytes < shm_config::MIN_HEAP_GROW_SIZE) {
        sk_info("fix allocation size: %lu.", bytes);
        bytes = shm_config::MIN_HEAP_GROW_SIZE;
    }

    if ((bytes & (shm_config::ALIGNMENT - 1)) != 0) {
        sk_info("fix allocation alignment: %lu.", bytes);
        bytes +=  shm_config::ALIGNMENT - 1;
        bytes >>= shm_config::ALIGNMENT_BITS;
        bytes <<= shm_config::ALIGNMENT_BITS;
    }

    block_t block = block_slots_.find_first();
    if (block >= block_slots_.size()) {
        sk_fatal("blocks used up.");
        return nullptr;
    }

    if (blocks_[block].addr && blocks_[block].size > 0) {
        sk_assert(0);
        block_slots_.reset(block);
        return nullptr;
    }

    char path[shm_config::MAX_PATH_SIZE];
    calc_path(block, path, sizeof(path));

    shm_segment seg;
    int ret = seg.init(path, bytes, shm_config::ALIGNMENT, nullptr, false);
    if (ret != 0) {
        sk_error("cannot init shm %s, ret: %d.", path, ret);
        return nullptr;
    }

    blocks_[block].addr    = seg.address();
    blocks_[block].id      = block;
    blocks_[block].size    = seg.size();
    blocks_[block].mapping = 0;
    block_slots_.reset(block);
    seg.release();

    return &(blocks_[block]);
}

const shm_block *block_mgr::find_block(void *ptr) const {
    block_t block = 0;
    bool exists = get_block_map(ptr, &block);

    // TODO: should we try looping the blocks to find the mapping here?
    if (!exists) {
        sk_error("ptr<%p> not managed.", ptr);
        return nullptr;
    }

    assert_retval(!block_slots_.test(block), nullptr);
    assert_retval(blocks_[block].id == block, nullptr);
    assert_retval(blocks_[block].addr <= ptr, nullptr);

    return &(blocks_[block]);
}

const shm_block *block_mgr::get_block(block_t block) const {
    assert_retval(!block_slots_.test(block), nullptr);
    assert_retval(blocks_[block].id == block, nullptr);

    return &(blocks_[block]);
}

void block_mgr::register_block(block_t block, bool force) {
    assert_retnone(!block_slots_.test(block));
    assert_retnone(blocks_[block].id == block);

    if (blocks_[block].mapping && !force) {
        sk_warn("block<%lu> has been registered.", block);
        return;
    }

    char *addr = char_ptr(blocks_[block].addr);
    char *end  = addr + blocks_[block].size;
    sk_assert((reinterpret_cast<uintptr_t>(addr) & (shm_config::ALIGNMENT - 1)) == 0);
    sk_assert((reinterpret_cast<uintptr_t>(end)  & (shm_config::ALIGNMENT - 1)) == 0);

    while (addr < end) {
        set_block_map(addr, block);
        addr += shm_config::ALIGNMENT;
    }
    sk_assert(addr == end);

    blocks_[block].mapping = 1;
}

bool block_mgr::get_block_map(void *ptr, block_t *block) const {
    size_t key = reinterpret_cast<size_t>(ptr);
    assert_retval((key >> shm_config::ADDRESS_BITS) == 0, false);

    key >>= shm_config::ALIGNMENT_BITS;
    const u16 *id = block_map_.get(key);
    if (!id) return false;

    if (block) *block = *id;
    return true;
}

void block_mgr::set_block_map(void *ptr, block_t block) {
    size_t key = reinterpret_cast<size_t>(ptr);
    assert_retnone((key >> shm_config::ADDRESS_BITS) == 0);
    assert_retnone((block >> (sizeof(u16) * 8)) == 0);

    key >>= shm_config::ALIGNMENT_BITS;
    block_map_.set(key, cast_u16(block));
}
