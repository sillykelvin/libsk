#ifndef PAGE_MAP_H
#define PAGE_MAP_H

#include "libsk.h"

namespace sk {
namespace detail {

/**
 * This is a two-level radix tree to represent page -> span mapping relationship.
 */
struct page_map {
    // 32 entries in the root, and (2^PREFIX_BITS) / 32 entries in each leaf.
    static const int PREFIX_BITS = MAX_MEM_SHIFT - PAGE_SHIFT;
    static const int ROOT_BITS   = 5;
    static const int LEAF_BITS   = PREFIX_BITS - ROOT_BITS;
    static const int ROOT_LENGTH = 1 << ROOT_BITS;
    static const int LEAF_LENGTH = 1 << LEAF_BITS;

    struct leaf {
        offset_t values[LEAF_LENGTH];
    };

    offset_t root[ROOT_LENGTH];

    void init() {
        memset(&root, 0x00, sizeof(root));
    }

    offset_t get(page_t p) {
        const size_t i1 = p >> LEAF_BITS;
        const size_t i2 = p & (LEAF_LENGTH - 1);

        // the page id exceeds max value
        if ((p >> PREFIX_BITS) > 0)
            return OFFSET_NULL;

        if (root[i1] == OFFSET_NULL)
            return OFFSET_NULL;

        // TODO: refine here
        void *addr = shm_mgr::get()->offset2ptr(root[i1]);
        if (!addr)
            return OFFSET_NULL;

        return cast_ptr(leaf, addr)->values[i2];
    }

    void set(page_t p, offset_t o) {
        const size_t i1 = p >> LEAF_BITS;
        const size_t i2 = p & (LEAF_LENGTH - 1);
        assert_retnone(i1 < (size_t) ROOT_LENGTH);

        if (root[i1] == OFFSET_NULL) {
            // TODO: allocate memory here
        }

        // TODO: refine here
        void *addr = shm_mgr::get()->offset2ptr(root[i1]);
        assert_retnone(addr);

        cast_ptr(leaf, addr)->values[i2] = o;
    }
};

} // namespace detail
} // namespace sk

#endif // PAGE_MAP_H
