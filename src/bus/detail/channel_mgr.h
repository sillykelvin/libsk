#ifndef CHANNEL_MGR_H
#define CHANNEL_MGR_H

#include "common/spin_lock.h"
#include "utility/types.h"
#include "bus/detail/channel.h"

namespace sk {
namespace detail {

struct channel_descriptor {
    int owner;         // owner bus id of this channel
    int closed;        // close the R/W channels if the owner is stopped
    pid_t pid;         // pid of the channel owner process
    size_t r_offset;   // offset of read channel
    size_t w_offset;   // offset of write channel
};

struct channel_mgr {
    static const int MAX_DESCRIPTOR_COUNT = 128;

    int magic;
    int shmid;        // id of this shm segment
    pid_t pid;        // pid of the busd process
    size_t shm_size;  // total size of this shm segment
    size_t used_size; // allocated space size

    spin_lock lock; // lock for multi process registration
    int descriptor_count;
    channel_descriptor descriptors[MAX_DESCRIPTOR_COUNT];

    /*
     * this function will be called and only be called in busd process
     */
    int init(int shmid, size_t shm_size, bool resume);

    void report() const;

    /*
     * these two functions will be called in each process,
     * thus we need to lock to ensure synchronization
     */
    int register_channel(int busid, pid_t pid, size_t node_size, size_t node_count, int& fd);
    void deregister_channel(int busid);

    channel *get_read_channel(int fd);
    channel *get_write_channel(int fd);
    const channel *get_read_channel(int fd) const;
    const channel *get_write_channel(int fd) const;
    int get_owner_busid(int fd) const;

    channel *find_read_channel(int busid, int& fd);
};

} // namespace detail
} // namespace sk

#endif // CHANNEL_MGR_H
