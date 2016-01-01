#include "libsk.h"
#include "page_map.h"

namespace sk {
namespace detail {

offset_t page_map::get(page_t p) const {
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

void page_map::set(page_t p, offset_t o) {
    const size_t i1 = p >> LEAF_BITS;
    const size_t i2 = p & (LEAF_LENGTH - 1);
    assert_retnone(i1 < (size_t) ROOT_LENGTH);

    if (root[i1] == OFFSET_NULL) {
        void *addr = shm_mgr::get()->allocate_metadata(sizeof(leaf), &root[i1]);
        assert_retnone(addr);

        memset(addr, 0x00, sizeof(leaf));
    }

    // TODO: refine here
    void *addr = shm_mgr::get()->offset2ptr(root[i1]);
    assert_retnone(addr);

    cast_ptr(leaf, addr)->values[i2] = o;
}

} // namespace detail
} // namespace sk
