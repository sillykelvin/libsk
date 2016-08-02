#ifndef CHANNEL_MGR_H
#define CHANNEL_MGR_H

#include "common/spin_lock.h"
#include "utility/types.h"

namespace sk {
namespace detail {

struct channel_message {
    size_t node_count; // how many nodes this message occupies
    char data[0];      // real message follows this struct
};

struct channel {
    int shmid;         // id of this shm segment
    key_t shm_key;     // shm key to this channel
    size_t node_count; // total node count of this channel
    size_t read_pos;   // current read position
    size_t write_pos;  // current write position
    void *addr;        // the start address of this channel

    int init(key_t shm_key, size_t node_size, size_t node_count, bool resume);
    int fini();

    int push(const void *data, size_t length);
    int pop(void *data, size_t max_length, size_t *real_length);

    bool empty() const;
};

struct channel_descriptor {
    int owner;         // owner bus id of this channel
    offset_t r_offset; // offset of read channel
    offset_t w_offset; // offset of write channel
};

struct channel_mgr {
    static const int MAX_DESCRIPTOR_COUNT = 128;

    int magic;
    int shmid;               // id of this shm segment
    size_t shm_size;         // total size of this shm segment
    size_t used_size;        // allocated space size

    spin_lock lock; // lock for multi process registration
    int descriptor_count;
    channel_descriptor descriptors[MAX_DESCRIPTOR_COUNT];

    /*
     * this function will be called and only be called in busd process
     */
    int init(int shmid, size_t shm_size, bool resume);

    /*
     * this function will be called in each process, thus we need to lock to
     * ensure synchronization
     */
    int register_channel(int busid, size_t node_size, size_t node_count, int& fd);
};

} // namespace detail
} // namespace sk

#endif // CHANNEL_MGR_H
