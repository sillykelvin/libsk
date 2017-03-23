#ifndef SHM_SEGMENT_H
#define SHM_SEGMENT_H

namespace sk {
namespace detail {

struct shm_segment {
    void *base_addr;
    int shmid;
    bool owner;

    shm_segment() : base_addr(nullptr), shmid(-1), owner(true) {}
    ~shm_segment() { if (owner && shmid != -1) fini(); }

    int __create(key_t key, size_t size);

    int __attach(key_t key);

    int init(key_t key, size_t size, bool resume);

    void fini();

    /*
     * if the shared memory is released from this segment, it
     * will NOT be destroyed when the segment is destructed.
     */
    void release() {
        owner = false;
    }

    void *address() {
        return base_addr;
    }
};

} // namespace detail
} // namespace sk

#endif // SHM_SEGMENT_H
