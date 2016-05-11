#include "libsk.h"
#include "span.h"

namespace sk {
namespace detail {

void span::init(page_t start, int count) {
    this->in_use     = false;
    this->start      = start;
    this->count      = count;
    this->prev       = SHM_NULL;
    this->next       = SHM_NULL;
    this->chunk_list = SHM_NULL;
    this->used_count = 0;
    this->size_class = -1;
}

void span::partition(size_t bytes, int size_class) {
    assert_retnone(bytes >= sizeof(shm_ptr<void>));
    assert_retnone(!chunk_list);
    assert_retnone(used_count == 0);
    assert_retnone(this->size_class == -1);

    this->size_class = size_class;

    offset_t offset = start << PAGE_SHIFT;
    offset_t end = (start + count) << PAGE_SHIFT;
    shm_ptr<void> *tail = &chunk_list;

    // here the condition cannot be "offset < end", because there
    // may be some extra space at the end, but the extra space
    // cannot hold size "bytes", so it will overflow to next page
    while (offset + bytes <= end) {
        shm_ptr<void> ptr(offset);
        *tail = ptr;
        tail = reinterpret_cast<shm_ptr<void>*>(ptr.get());
        offset += bytes;
    }
    *tail = SHM_NULL;
}

void span::erase() {
    chunk_list = SHM_NULL;
    used_count = 0;
    size_class = -1;
}

} // namespace detail
} // namespace sk
