#ifndef CHANNEL_H
#define CHANNEL_H

#include "utility/types.h"

namespace sk {
namespace detail {

struct channel {
    int magic;
    size_t node_count;    // total node count of this channel
    size_t read_pos;      // current read position
    size_t write_pos;     // current write position
    offset_t node_offset; // offset of the first node to pointer this

    int init(size_t node_count);

    int push(const void *data, size_t length);
    int pop(void *data, size_t max_length, size_t *real_length);

    bool empty() const;
};

} // namespace detail
} // namespace sk

#endif // CHANNEL_H
