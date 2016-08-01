#ifndef BUS_H
#define BUS_H

#include "common/spin_lock.h"
#include "utility/types.h"

NS_BEGIN(sk)
NS_BEGIN(detail)

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

NS_END(detail)

struct channel_mgr {
    static const int MAX_CHANNEL_COUNT = 128;

    int magic;
    int shmid;              // id of this shm segment
    size_t node_count;      // how many nodes a channel owns
    size_t node_size;       // the size of a node
    size_t node_size_shift; // 2 ^ node_size_shift = node_size

    spin_lock lock; // lock channel_count for multi process registration
    size_t channel_count;
    detail::channel channel_list[MAX_CHANNEL_COUNT];

    int init(int shmid, size_t node_size, size_t node_count, bool resume);
    int fini();

    size_t calc_node_count(size_t length);

    int register_channel(key_t shm_key, size_t& handle);
};

NS_END(sk)

#endif // BUS_H
