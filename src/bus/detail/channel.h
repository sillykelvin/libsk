#ifndef CHANNEL_H
#define CHANNEL_H

#include "utility/types.h"

namespace sk {
namespace detail {

struct channel_message {
    int src_busid;     // which process this message comes from
    int dst_busid;     // which process this message goes to
    size_t node_count; // how many nodes this message occupies
    char data[0];      // real message follows this struct
};

struct channel {
    int magic;
    size_t node_count;      // total node count of this channel
    size_t node_size;       // the size of a node
    size_t node_size_shift; // 2 ^ node_size_shift = node_size
    size_t read_pos;        // current read position
    size_t write_pos;       // current write position
    offset_t node_offset;   // offset of the first node

    int init(size_t node_size, size_t node_count);

    int push(const void *data, size_t length);
    int pop(void *data, size_t max_length, size_t *real_length);

    bool empty() const;
};

} // namespace detail
} // namespace sk

#endif // CHANNEL_H
