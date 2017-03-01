#ifndef CHANNEL_H
#define CHANNEL_H

#include "utility/types.h"

namespace sk {
namespace detail {

struct channel_message {
    int magic;
    u32 hash;       // hash value of the data block, for verification
    int src_busid;  // which process this message comes from
    int dst_busid;  // which process this message goes to
    size_t length;  // message length
    char data[0];   // real message follows this struct
};

struct channel {
    int magic;
    size_t node_count;          // total node count of this channel
    size_t node_size;           // the size of a node
    size_t node_size_shift;     // 2 ^ node_size_shift = node_size
    volatile size_t push_count; // total message count pushed to this channel
    volatile size_t pop_count;  // total message count popped from this channel
    volatile size_t read_pos;   // current read position
    volatile size_t write_pos;  // current write position
    offset_t node_offset;       // offset of the first node

    static size_t calc_space(size_t node_size, size_t node_count) {
        return sizeof(channel) + node_size * node_count;
    }

    int init(size_t node_size, size_t node_count);

    void clear();

    /**
     * @brief push a message into the channel
     * @param src_busid: from which bus this message is sent
     * @param dst_busid: which bus this message is sent to
     * @param data: the message data
     * @param length: the length of the message
     * @return 0 if succeeds, error code otherwise
     */
    int push(int src_busid, int dst_busid, const void *data, size_t length);

    /**
     * @brief pop a message from the channel
     * @param data: buffer to store the popped message, if it's NULL, channel
     *              will drop this message
     * @param length: length of the buffer, also stores the length of the
     *                message, if data is NULL, this field is discarded
     * @param src_busid: store source bus id, can be NULL
     * @param dst_busid: store destination bus id, can be NULL
     * @return count of popped message, should be 0 or 1, or negative error code:
     *         1. -E2BIG: the message is too big to store in provided buffer
     *         2. -1: assert error
     */
    int pop(void *data, size_t& length, int *src_busid, int *dst_busid);

    size_t message_count() const;

    size_t __calc_node_count(size_t data_len) const;

    channel_message *__channel_message(size_t pos);
};

} // namespace detail
} // namespace sk

#endif // CHANNEL_H
