#ifndef PAGE_MAP_H
#define PAGE_MAP_H

#include "utility/types.h"
#include "utility/config.h"
#include "shm/detail/offset_ptr.h"

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
        offset_ptr<void> values[LEAF_LENGTH];
    };

    offset_ptr<leaf> root[ROOT_LENGTH];

    /*
     * estimate the metadata space needed to store page -> span
     * mapping info by given page count
     */
    static size_t estimate_space(int page_count);

    void init();

    offset_ptr<void> get(page_t p) const;

    void set(page_t p, offset_ptr<void> v);
};

} // namespace detail
} // namespace sk

#endif // PAGE_MAP_H
