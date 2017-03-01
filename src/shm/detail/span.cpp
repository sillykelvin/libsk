#include "libsk.h"
#include "span.h"

namespace sk {
namespace detail {

void span::init(page_t start, int count) {
    this->in_use     = false;
    this->start      = start;
    this->count      = count;
    this->used_count = 0;
    this->size_class = -1;
    this->prev.set_null();
    this->next.set_null();
    this->chunk_list.set_null();
}

void span::partition(size_t bytes, int size_class) {
    assert_retnone(bytes >= sizeof(offset_ptr<void>));
    assert_retnone(!chunk_list);
    assert_retnone(used_count == 0);
    assert_retnone(this->size_class == -1);

    this->size_class = size_class;

    offset_t offset = start << PAGE_SHIFT;
    offset_t end = (start + count) << PAGE_SHIFT;
    offset_ptr<void> *tail = &chunk_list;

    // here the condition cannot be "offset < end", because there
    // may be some extra space at the end, but the extra space
    // cannot hold size "bytes", so it will overflow to next page
    while (offset + bytes <= end) {
        offset_ptr<void> ptr(offset);
        *tail = ptr;
        tail = static_cast<offset_ptr<void>*>(ptr.get());
        offset += bytes;
    }
    tail->set_null();
}

void span::erase() {
    used_count = 0;
    size_class = -1;
    chunk_list.set_null();
}

} // namespace detail
} // namespace sk
