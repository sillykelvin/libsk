#ifndef PAGE_MAP_H
#define PAGE_MAP_H

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

    offset_t get(page_t p) const;

    void set(page_t p, offset_t o);
};

} // namespace detail
} // namespace sk

#endif // PAGE_MAP_H
