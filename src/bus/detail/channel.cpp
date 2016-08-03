#include "channel.h"
#include "shm/detail/shm_segment.h"
#include "utility/assert_helper.h"
#include "utility/config.h"

namespace sk {
namespace detail {

int channel::init(size_t node_size, size_t node_count) {
    assert_retval(node_size > 0, -1);
    assert_retval(node_count > 0, -1);
    assert_retval(node_size & (node_size - 1) == 0, -1);

    this->magic = MAGIC;
    this->node_count = node_count;
    this->node_size = node_size;
    this->read_pos = 0;
    this->write_pos = 0;
    // TODO: may do some alignment job here
    this->node_offset = sizeof(channel);

    this->node_size_shift = 0;
    while (node_size > 1) {
        node_size = node_size >> 1;
        ++this->node_size_shift;
    }

    return 0;
}

int channel::push(int src_busid, int dst_busid, const void *data, size_t length) {
    assert_retval(magic == MAGIC, -1);

    // TODO: implementation
    return 0;
}

int channel::pop(void *data, size_t& length, int *src_busid, int *dst_busid) {
    assert_retval(magic == MAGIC, -1);

    // TODO: implementation
    return 0;
}

bool channel::empty() const {
    // TODO: implementation
    return 0;
}

size_t channel::__calc_node_count(size_t data_len) const {
    size_t total_len = sizeof(channel_message) + data_len;
    return ((total_len - 1) >> node_size_shift) + 1;
}

} // namespace detail
} // namespace sk
