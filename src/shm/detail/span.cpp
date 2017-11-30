#include <shm/detail/span.h>

using namespace sk::detail;

void span::partition(size_t bytes, u8 size_class) {
    assert_retnone(bytes >= sizeof(shm_address));
    assert_retnone(!chunk_list);
    assert_retnone(used_count == 0);
    assert_retnone(this->size_class == -1);

    this->size_class = size_class;

    shm_address *tail = &chunk_list;
    offset_t offset   = start_page << shm_config::PAGE_SHIFT;
    offset_t end      = (start_page + page_count) << shm_config::PAGE_SHIFT;

    // NOTE: here the condition cannot be "while (offset < end)", because there
    // might be some extra space at the end and the extra space cannot hold the
    // "bytes" size, so it will overflow to next page
    while (offset + bytes <= end) {
        shm_address addr(block, offset);
        *tail = addr;
        tail  = addr.as<shm_address>();
        offset += bytes;
    }

    *tail = nullptr;
}

void span::erase() {
    used_count = 0;
    size_class = -1;
    chunk_list = nullptr;
}
