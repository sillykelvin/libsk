#include "libsk.h"
#include "page_map.h"

namespace sk {
namespace detail {

size_t page_map::estimate_space(int page_count) {
    const page_t last_page = page_count - 1;
    const size_t i1 = last_page >> LEAF_BITS;

    return (i1 + 1) * sizeof(leaf);
}

void page_map::init() {
    memset(root, 0x00, sizeof(root));
}

shm_ptr<void> page_map::get(page_t p) const {
    const size_t i1 = p >> LEAF_BITS;
    const size_t i2 = p & (LEAF_LENGTH - 1);

    // the page id exceeds max value
    if ((p >> PREFIX_BITS) > 0)
        return SHM_NULL;

    if (!root[i1])
        return SHM_NULL;

    return root[i1]->values[i2];
}

void page_map::set(page_t p, shm_ptr<void> v) {
    const size_t i1 = p >> LEAF_BITS;
    const size_t i2 = p & (LEAF_LENGTH - 1);
    assert_retnone(i1 < (size_t) ROOT_LENGTH);

    if (!root[i1]) {
        shm_ptr<void> ptr(shm_mgr::get()->allocate_metadata(sizeof(leaf)));
        assert_retnone(ptr);

        root[i1] = ptr;
        leaf *addr = root[i1].get();
        memset(addr, 0x00, sizeof(*addr));
    }

    root[i1]->values[i2] = v;
}

} // namespace detail
} // namespace sk
