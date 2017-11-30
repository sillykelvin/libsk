#include <sys/mman.h>
#include <sys/file.h>
#include <utility/assert_helper.h>
#include <shm/detail/shm_segment.h>

using namespace sk::detail;

int shm_segment::init(const char *path, size_t size,
                      size_t alignment, void *fixed_addr, bool resume_mode) {
    const size_t page_size = cast_size(getpagesize());

    // alignment should be at least page_size, as the mmap()
    // sys call will return the address aligned by page_size
    if (alignment < page_size) alignment = page_size;

    // size is not aligned, we need to revise that
    if ((size & (alignment - 1)) != 0) {
        size_t new_size = size + (alignment - (size & (alignment - 1)));
        sk_assert((new_size & (alignment - 1)) == 0);

        if (new_size < size)
            return -EOVERFLOW;

        size = new_size;
    }

    return resume_mode ? attach(path, size, page_size, alignment, fixed_addr)
                       : create(path, size, page_size, alignment, fixed_addr);
}

int shm_segment::fini() {
    if (addr_) {
        int ret = munmap(addr_, size_);
        sk_assert(ret == 0);

        addr_ = nullptr;
        size_ = 0;
    }

    if (!path_.empty()) {
        int ret = shm_unlink(path_.c_str());
        sk_assert(ret == 0);

        path_.clear();
    }

    return 0;
}

int shm_segment::create(const char *path, size_t size,
                        size_t page_size, size_t alignment, void *fixed_addr) {
    int shmfd = shm_open(path, O_CREAT | O_TRUNC | O_RDWR, 0666);
    if (shmfd == -1) {
        sk_error("shm_open() error: %s.", strerror(errno));
        return -errno;
    }

    // NOTE: we only allocate size bytes here but not size + extra bytes,
    // see the comments of mmap() call in do_mmap() for the detail reason
    int ret = ftruncate(shmfd, cast_s64(size));
    if (ret != 0) {
        sk_error("ftruncate() error: %s.", strerror(errno));
        return -errno;
    }

    return do_mmap(path, size, page_size, alignment, shmfd, fixed_addr);
}

int shm_segment::attach(const char *path, size_t size,
                        size_t page_size, size_t alignment, void *fixed_addr) {
    int shmfd = shm_open(path, O_RDWR, 0666);
    if (shmfd == -1) {
        sk_error("shm_open() error: %s.", strerror(errno));
        return -errno;
    }

    struct stat st;
    int ret = fstat(shmfd, &st);
    if (ret != 0) {
        sk_error("fstat() error: %s.", strerror(errno));
        return -errno;
    }

    if (cast_size(st.st_size) < size) {
        sk_error("shm<%s> size<%ld> not enough, size<%lu>.", path, st.st_size, size);
        return -EINVAL;
    }

    return do_mmap(path, size, page_size, alignment, shmfd, fixed_addr);
}

int shm_segment::do_mmap(const char *path, size_t size, size_t page_size,
                         size_t alignment, int shmfd, void *fixed_addr) {
    void *addr = nullptr;

    // if the preferred address is provided and it's valid, we then try to map
    // to that address
    if (fixed_addr) {
        if (unlikely((reinterpret_cast<uintptr_t>(fixed_addr) & (alignment - 1)) != 0)) {
            sk_error("the given address<%p> is not aligned, alignment<%lu>.", fixed_addr, alignment);
        } else {
            addr = mmap(fixed_addr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, shmfd, 0);
            if (addr == MAP_FAILED) {
                sk_warn("cannot map to fixed address<%p>, error<%s>, size<%lu>, alignment<%lu>.",
                        fixed_addr, strerror(errno), size, alignment);
                addr = nullptr;
            } else {
                sk_assert(addr == fixed_addr);
            }
        }
    }

    /*
     * if the preferred address is not provided, or it's invalid, or map to it
     * failed, then we have to find an available aligned mapping point
     *
     * if we ftruncate the shmfd to size + extra, we will waste extra bytes of
     * memory, however, the mmap supports that the size exceeds the real memory
     * size(but writing to the extra mapping area will receive a SEGFAULT), as
     * we only do this mmap to find an aligned mapping point, it's ok to exceeds
     * the real size, after the aligned mapping point is found, we will map to
     * that mapping point later
     */
    if (!addr) {
        size_t extra = 0;
        if (alignment > page_size) extra = alignment - page_size;

        addr = mmap(nullptr, size + extra, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
        if (addr == MAP_FAILED) {
            sk_error("mmap() error: %s.", strerror(errno));
            return -errno;
        }

        size_t skip_size = 0;
        uintptr_t ptr = reinterpret_cast<uintptr_t>(addr);
        if ((ptr & (alignment - 1)) != 0) {
            skip_size = alignment - (ptr & (alignment - 1));
            sk_assert(skip_size <= extra);
        }

        if (skip_size <= 0) {
            // the address happens to be aligned, then we just need to release
            // the extra mapped space
            if (extra > 0) {
                int ret = munmap(reinterpret_cast<void*>(ptr + size), extra);
                sk_assert(ret == 0);
            }
        } else {
            // the address is not aligned, we need to unmap the entire space, and
            // then map it at the aligned fixed address, as the fixed address and
            // size is smaller than the previous mmap space, so the following mmap
            // with MAP_FIXED should succeed, if it does not, FML!!
            int ret = munmap(addr, size + extra);
            sk_assert(ret == 0);

            addr = mmap(reinterpret_cast<void*>(ptr + skip_size), size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, shmfd, 0);
            if (addr == MAP_FAILED) {
                sk_error("mmap() error: %s, previous<%lu:%lu>, revised<%lu:%lu>.",
                         strerror(errno), ptr, size + extra, ptr + skip_size, size);
                return -errno;
            }

            sk_assert(addr == reinterpret_cast<void*>(ptr + skip_size));
        }
    }

    sk_assert(addr && ((reinterpret_cast<uintptr_t>(addr) & (alignment - 1)) == 0));
    close(shmfd);
    addr_ = addr;
    size_ = size;
    path_ = path;

    return 0;
}
