#include <shm/detail/span.h>

using namespace sk::detail;

void span::partition(size_t bytes, u8 size_class) {
    assert_retnone(bytes >= sizeof(shm_address));
    assert_retnone(!chunk_list_);
    assert_retnone(used_count_ == 0);
    assert_retnone(this->size_class_ == cast_u8(-1));

    this->size_class_ = size_class;

    shm_address *tail   = &chunk_list_;
    shm_offset_t offset = start_page_ << shm_config::PAGE_BITS;
    shm_offset_t end    = (start_page_ + page_count_) << shm_config::PAGE_BITS;

    // NOTE: here the condition cannot be "while (offset < end)", because there
    // might be some extra space at the end and the extra space cannot hold the
    // "bytes" size, so it will overflow to next page
    while (offset + bytes <= end) {
        shm_address addr(shm_config::USERDATA_SERIAL_NUM, offset);
        *tail = addr;
        tail  = addr.as<shm_address>();
        offset += bytes;
    }

    *tail = nullptr;
}

void span::erase() {
    used_count_ = 0;
    size_class_ = cast_u8(-1);
    chunk_list_ = nullptr;
}

shm_address span::fetch() {
    shm_address ret = chunk_list_;
    if (ret) {
        chunk_list_ = *(chunk_list_.as<shm_address>());
        used_count_ += 1;
    }

    return ret;
}

void span::recycle(shm_address chunk) {
    *(chunk.as<shm_address>()) = chunk_list_;
    chunk_list_ = chunk;
    used_count_ -= 1;
}
