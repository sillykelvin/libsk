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

offset_ptr<void> page_map::get(page_t p) const {
    const size_t i1 = p >> LEAF_BITS;
    const size_t i2 = p & (LEAF_LENGTH - 1);

    // the page id exceeds max value
    if ((p >> PREFIX_BITS) > 0)
        return offset_ptr<void>::null();

    if (!root[i1])
        return offset_ptr<void>::null();

    leaf *l = root[i1].get();
    return l->values[i2];
}

void page_map::set(page_t p, offset_ptr<void> v) {
    const size_t i1 = p >> LEAF_BITS;
    const size_t i2 = p & (LEAF_LENGTH - 1);
    assert_retnone(i1 < (size_t) ROOT_LENGTH);

    leaf *addr = NULL;
    if (!root[i1]) {
        offset_ptr<leaf> ptr(shm_mgr::get()->allocate_metadata(sizeof(leaf)));
        assert_retnone(ptr);

        root[i1] = ptr;
        addr = root[i1].get();
        memset(addr, 0x00, sizeof(*addr));
    }

    if (!addr)
        addr = root[i1].get();

    addr->values[i2] = v;
}

} // namespace detail
} // namespace sk
