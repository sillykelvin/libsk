#include <string.h>
#include "channel.h"
#include "log/log.h"
#include "shm/detail/shm_segment.h"
#include "utility/assert_helper.h"
#include "utility/config.h"
#include "utility/math_helper.h"
#include "common/murmurhash3.h"

#define MURMURHASH_SEED 77

NS_BEGIN(sk)
NS_BEGIN(detail)

int channel::init(size_t node_size, size_t node_count) {
    // node_count should > 1 because there will be an empty
    // node to distinguish a full channel or an empty channel:
    // 1. if it's an empty channel, then read_pos == write_pos
    // 2. if it's a full channel, then write_pos + 1 == read_pos
    assert_retval(node_count > 1, -1);
    assert_retval((node_size & (node_size - 1)) == 0, -1);
    assert_retval(node_size >= sizeof(channel_message), -1);

    this->magic = MAGIC;
    this->node_count = node_count;
    this->node_size = node_size;
    this->push_count = 0;
    this->pop_count  = 0;
    this->read_pos   = 0;
    this->write_pos  = 0;
    this->node_offset = sizeof(channel);

    this->node_size_shift = 0;
    while (node_size > 1) {
        node_size = node_size >> 1;
        ++this->node_size_shift;
    }

    return 0;
}

void channel::clear() {
    assert_retnone(magic == MAGIC);

    this->push_count = 0;
    this->pop_count  = 0;
    this->read_pos   = 0;
    this->write_pos  = 0;
}

int channel::push(int src_busid, int dst_busid, u64 ctime, const void *data, size_t length) {
    assert_retval(magic == MAGIC, -1);

    if (!data || length <= 0)
        return 0;

    const size_t required_count = __calc_node_count(length);
    assert_retval(length + sizeof(channel_message) <= required_count * node_size, -1);

    // reserve a node to distinguish a full channel from an empty channel, so minus 1 here
    const size_t available_count = (read_pos - write_pos + node_count - 1) % node_count;
    const size_t new_write_pos = (write_pos + required_count) % node_count;

    if (required_count > available_count) {
        sk_error("no enough space for incoming message, required<%lu>, available<%lu>.",
                 required_count, available_count);
        return -ENOMEM;
    }

    channel_message *head = __channel_message(write_pos);

    // loop back
    if (new_write_pos > 0 && new_write_pos < write_pos) {
        void *addr0 = void_ptr(head->data);
        size_t sz0 = (node_count - write_pos) * node_size - sizeof(*head);
        void *addr1 = sk::byte_offset<void>(this, node_offset);
        size_t sz1 = new_write_pos * node_size;

        // this should NOT happen, if it happens, then function
        // __calc_node_count(...) must have a bug
        if (sz0 >= length) {
            sk_assert(0);
            memcpy(addr0, data, length);
        } else {
            sk_assert(length - sz0 <= sz1);
            memcpy(addr0, data, sz0);
            memcpy(addr1, sk::byte_offset<void>(data, sz0), length - sz0);
        }
    } else {
        void *addr = void_ptr(head->data);
        memcpy(addr, data, length);
    }

    head->magic = MAGIC;
    head->src_busid = src_busid;
    head->dst_busid = dst_busid;
    head->length = length;
    head->ctime = ctime;
    sk::murmurhash3_x86_32(data, length, MURMURHASH_SEED, &head->hash);

    // start a full memory barrier here
    __sync_synchronize();

    write_pos = new_write_pos;
    push_count += 1;
    return 0;
}

int channel::pop(void *data, size_t& length, int *src_busid, int *dst_busid, u64 *ctime) {
    assert_retval(magic == MAGIC, -1);

    // no data
    if (read_pos == write_pos) return 0;

    const channel_message *head = __channel_message(read_pos);
    assert_retval(head->magic == MAGIC, -1);
    assert_retval(head->length > 0, -1);

    const size_t used_count = __calc_node_count(head->length);
    const size_t new_read_pos = (read_pos + used_count) % node_count;
    if (new_read_pos != write_pos) {
        const channel_message *h = __channel_message(new_read_pos);
        sk_assert(h->magic == MAGIC);
        sk_assert(h->length > 0);
    }

    if (data) {
        if (head->length > length) {
            length = head->length;

            sk_error("buffer too small, required size<%lu>.", head->length);
            return -E2BIG;
        }

        // loop back
        if (new_read_pos > 0 && new_read_pos < read_pos) {
            void *addr0 = void_ptr(const_cast<char*>(head->data));
            size_t sz0 = (node_count - read_pos) * node_size - sizeof(*head);
            void *addr1 = sk::byte_offset<void>(this, node_offset);
            size_t sz1 = new_read_pos * node_size;

            // this should NOT happen
            if (sz0 >= head->length) {
                sk_assert(0);
                memcpy(data, addr0, head->length);
            } else {
                sk_assert(head->length - sz0 <= sz1);
                memcpy(data, addr0, sz0);
                memcpy(sk::byte_offset<void>(data, sz0), addr1, head->length - sz0);
            }
        } else {
            void *addr = void_ptr(const_cast<char*>(head->data));
            memcpy(data, addr, head->length);
        }

        length = head->length;

        u32 hash = 0;
        sk::murmurhash3_x86_32(data, length, MURMURHASH_SEED, &hash);
        assert_retval(hash == head->hash, -1);
    }

    if (src_busid) *src_busid = head->src_busid;
    if (dst_busid) *dst_busid = head->dst_busid;
    if (ctime)     *ctime     = head->ctime;

    // start a full memory barrier here
    __sync_synchronize();

    read_pos = new_read_pos;
    pop_count += 1;
    return 1;
}

size_t channel::message_count() const {
    if (push_count >= pop_count)
        return push_count - pop_count;

    // this may happen under two situations:
    // 0. push_count reaches maximum value of size_t and returns to 0
    // 1. a synchronize issue due to push_count/pop_count are manipulated
    //    by two processes
    // however, for both situations, we do not need to care much and do
    // not need any "fix", as this function is just a helper function,
    // a warning log should be enough
    sk_warn("incorrect push count<%d>, pop count<%d>.", push_count, pop_count);
    return 0;
}

size_t channel::__calc_node_count(size_t data_len) const {
    size_t total_len = sizeof(channel_message) + data_len;
    return ((total_len - 1) >> node_size_shift) + 1;
}

channel_message *channel::__channel_message(size_t pos) {
    if (pos >= node_count) return NULL;

    char *base_addr = sk::byte_offset<char>(this, node_offset);
    char *addr = base_addr + node_size * pos;
    return cast_ptr(channel_message, addr);
}

NS_END(sk)
NS_END(detail)
