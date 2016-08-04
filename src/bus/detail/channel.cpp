#include <errno.h>
#include "channel.h"
#include "shm/detail/shm_segment.h"
#include "utility/assert_helper.h"
#include "utility/config.h"
#include "common/murmurhash3.h"

#define MURMURHASH_SEED 77

namespace sk {
namespace detail {

int channel::init(size_t node_size, size_t node_count) {
    // node_count should > 1 because there will be an empty
    // node to distinguish a full channel or an empty channel:
    // 1. if it's an empty channel, then read_pos == write_pos
    // 2. if it's a full channel, then write_pos + 1 == read_pos
    assert_retval(node_count > 1, -1);
    assert_retval(node_size & (node_size - 1) == 0, -1);
    assert_retval(node_size >= sizeof(channel_message), -1);

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

    // TODO: refine this function, make it more robust

    if (!data || length <= 0)
        return 0;

    const size_t required_count = __calc_node_count(length);
    assert_retval(length + sizeof(channel_message) <= required_count * node_size, -1);

    const size_t available_count = __available_node_count();
    // TODO: retry or wait here?
    if (required_count > available_count) {
        error("no enough space for incoming message, required<%lu>, available<%lu>.",
              required_count, available_count);
        return -ENOMEM;
    }

    channel_message *head = cast_ptr(channel_message, char_ptr(this) + node_offset + write_pos * node_size);
    memset(head, 0x00, sizeof(*head));

    size_t new_write_pos = (write_pos + required_count) % node_count;

    // loop back
    if (new_write_pos > 0 && new_write_pos < write_pos) {
        void *addr0 = void_ptr(head->data);
        size_t sz0 = (node_count - write_pos) * node_size - sizeof(*head);
        void *addr1 = void_ptr(char_ptr(this) + node_offset);
        size_t sz1 = new_write_pos * node_size;

        // this should NOT happen, if it happens, then function
        // __calc_node_count(...) must have a bug
        if (sz0 >= length) {
            sk_assert(0);
            memcpy(addr0, data, length);
        } else {
            sk_assert(length - sz0 <= sz1);
            memcpy(addr0, data, sz0);
            memcpy(addr1, void_ptr(char_ptr(data) + sz0), length - sz0);
        }
    } else {
        void *addr = void_ptr(head->data);
        memcpy(addr, data, length);
    }

    head->src_busid = src_busid;
    head->dst_busid = dst_busid;
    head->node_count = required_count;
    sk::murmurhash3_x86_32(data, length, MURMURHASH_SEED, head->hash);

    // start a full memory barrier here
    __sync_synchronize();

    write_pos = new_write_pos;
    return 0;
}

int channel::pop(void *data, size_t& length, int *src_busid, int *dst_busid) {
    assert_retval(magic == MAGIC, -1);

    // TODO: refine this function, make it more robust

    // no data
    if (read_pos == write_pos) return 0;

    const channel_message *head = cast_ptr(channel_message, char_ptr(this) + node_offset + read_pos * node_size);
    assert_retval(head->node_count > 0, -1);

    size_t new_read_pos = (read_pos + head->node_count) % node_count;
    assert_retval(new_read_pos <= write_pos, -1);

    if (data) {
        size_t msg_size = head->node_count * node_size - sizeof(*head);
        if (msg_size > length) {
            length = msg_size;

            error("buffer too small, required size<%lu>.", msg_size);
            return -E2BIG;
        }

        // loop back
        if (new_read_pos > 0 && new_read_pos < read_pos) {
            void *addr0 = void_ptr(head->data);
            size_t sz0 = (node_count - read_pos) * node_size - sizeof(*head);
            void *addr1 = void_ptr(char_ptr(this) + node_offset);
            size_t sz1 = new_read_pos * node_size;

            // this should NOT happen
            if (sz0 >= msg_size) {
                sk_assert(0);
                memcpy(data, addr0, msg_size);
            } else {
                sk_assert(msg_size - sz0 <= sz1);
                memcpy(data, addr0, sz0);
                memcpy(void_ptr(char_ptr(data) + sz0), addr1, msg_size - sz0);
            }
        } else {
            void *addr = void_ptr(head->data);
            memcpy(data, addr, msg_size);
        }

        length = msg_size;

        u32 hash = 0;
        sk::murmurhash3_x86_32(data, length, MURMURHASH_SEED, hash);
        assert_retval(hash == head->hash, -1);
    }

    if (src_busid) *src_busid = head->src_busid;
    if (dst_busid) *dst_busid = head->dst_busid;

    // start a full memory barrier here
    __sync_synchronize();

    read_pos = new_read_pos;
    return 1;
}

size_t channel::__calc_node_count(size_t data_len) const {
    size_t total_len = sizeof(channel_message) + data_len;
    return ((total_len - 1) >> node_size_shift) + 1;
}

size_t channel::__available_node_count() const {
    return (read_pos - write_pos + node_count) % node_count;
    // if (write_pos >= read_pos)
    //     return node_count - write_pos + read_pos;
    // else
    //     return read_pos - write_pos;
}

} // namespace detail
} // namespace sk
