#include "channel.h"
#include "shm/detail/shm_segment.h"
#include "utility/assert_helper.h"
#include "utility/config.h"

namespace sk {
namespace detail {

int channel::init(size_t node_count) {
    assert_retval(node_count > 0, -1);

    this->magic = MAGIC;
    this->node_count = node_count;
    this->read_pos = 0;
    this->write_pos = 0;
    // TODO: may do some alignment job here
    this->node_offset = sizeof(channel);

    return 0;
}

int channel::push(const void *data, size_t length) {
    // TODO: implementation
    return 0;
}

int channel::pop(void *data, size_t max_length, size_t *real_length) {
    // TODO: implementation
    return 0;
}

bool channel::empty() const {
    // TODO: implementation
    return 0;
}

} // namespace detail
} // namespace sk
