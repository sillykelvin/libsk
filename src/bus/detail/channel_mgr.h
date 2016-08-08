#ifndef CHANNEL_MGR_H
#define CHANNEL_MGR_H

#include "common/spin_lock.h"
#include "utility/types.h"
#include "bus/detail/channel.h"

namespace sk {
namespace detail {

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

    channel *get_read_channel(int fd);
    channel *get_write_channel(int fd);
    int get_owner_busid(int fd);

    channel *find_read_channel(int busid);
};

} // namespace detail
} // namespace sk

#endif // CHANNEL_MGR_H
