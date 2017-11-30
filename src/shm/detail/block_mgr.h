#ifndef BLOCK_MGR_H
#define BLOCK_MGR_H

#include <shm/detail/radix_tree.h>
#include <shm/detail/shm_address.h>
#include <container/fixed_bitmap.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

struct shm_block {
    shm_block() : addr(nullptr), id(0), size(0), mapping(0), padding(0) {}

    void *addr;
    size_t id: shm_config::MAX_BLOCK_BITS;
    size_t size: shm_config::MAX_HEAP_GROW_BITS;
    size_t mapping: 1;
    size_t padding: 15;
};
static_assert(std::is_standard_layout<shm_block>::value, "invalid shm_block");
static_assert(sizeof(shm_block) == sizeof(void*) + sizeof(size_t), "invalid shm_block");

class block_mgr {
public:
    explicit block_mgr(const char *basename) {
        snprintf(basename_, sizeof(basename_), "%s", basename);
        block_slots_.set();
    }

    int on_resume(const char *basename, std::vector<block_t>& changed_blocks);

    const shm_block *allocate_block(size_t bytes);
    const shm_block *find_block(void *ptr) const;
    const shm_block *get_block(block_t block) const;
    void register_block(block_t block, bool force);

private:
    bool get_block_map(void *ptr, block_t *block) const;
    void set_block_map(void *ptr, block_t block);

    void calc_path(block_t block, char *buf, size_t bufsize) {
        snprintf(buf, bufsize, "%s-%05lu.mmap", basename_, block);
    }

private:
    static const size_t MAX_BITS = shm_config::ADDRESS_BITS - shm_config::ALIGNMENT_BITS;
    static const size_t LV0_BITS = MAX_BITS / 3;
    static const size_t LV1_BITS = MAX_BITS / 3;
    static const size_t LV2_BITS = MAX_BITS - (LV0_BITS + LV1_BITS);

    // make sure a block id can be stored in a "u16" in block_map_
    static_assert(shm_config::MAX_BLOCK_BITS <= sizeof(u16) * 8, "block overflow");

    char basename_[shm_config::MAX_PATH_SIZE];
    shm_block blocks_[shm_config::MAX_BLOCK_BITS];
    fixed_bitmap<array_len(blocks_)> block_slots_;
    radix_tree<u16, LV0_BITS, LV1_BITS, LV2_BITS> block_map_; // address -> block
};
static_assert(std::is_standard_layout<block_mgr>::value, "invalid block_mgr");

NS_END(detail)
NS_END(sk)

#endif // BLOCK_MGR_H
