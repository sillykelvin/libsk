#ifndef SHM_SEGMENT_H
#define SHM_SEGMENT_H

#include <string>
#include <utility/types.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

class shm_segment {
public:
    shm_segment() : owner_(true), addr_(nullptr), size_(0) {}
    ~shm_segment() { if (owner_) fini(); }

    MAKE_NONCOPYABLE(shm_segment);

    int init(const char *path, size_t size, size_t alignment, void *fixed_addr, bool resume_mode);
    int fini();

    void *address() const { return addr_; }
    size_t size()   const { return size_; }

    void release() { owner_ = false; }

private:
    int create(const char *path, size_t size, size_t page_size, size_t alignment, void *fixed_addr);
    int attach(const char *path, size_t size, size_t page_size, size_t alignment, void *fixed_addr);

    int do_mmap(const char *path, size_t size, size_t page_size, size_t alignment, int shmfd, void *fixed_addr);

private:
    bool owner_;
    void *addr_;
    size_t size_;
    std::string path_;
};

NS_END(detail)
NS_END(sk)

#endif // SHM_SEGMENT_H
