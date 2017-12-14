#include <sys/mman.h>
#include <sys/file.h>
#include <shm/detail/shm_object.h>
#include <utility/assert_helper.h>

int sk::detail::shm_object_create(const char *path, size_t *size) {
    int shmfd = shm_open(path, O_CREAT | O_EXCL | O_TRUNC | O_RDWR, 0666);
    if (shmfd == -1) {
        sk_error("shm_open() error: %s.", strerror(errno));
        return -1;
    }

    size_t page_size = cast_size(sysconf(_SC_PAGESIZE));
    size_t real_size = *size;
    if (real_size % page_size != 0) {
        real_size += page_size - (real_size & (page_size - 1));
        sk_assert(real_size % page_size == 0);
    }

    int ret = ftruncate(shmfd, cast_s64(real_size));
    if (ret != 0) {
        // save error code as close() and shm_unlink() might change it
        int error = errno;
        close(shmfd);
        shm_unlink(path);

        sk_error("ftruncate() error: %s.", strerror(error));
        errno = error;
        return -1;
    }

    *size = real_size;
    return shmfd;
}

int sk::detail::shm_object_attach(const char *path, size_t *size) {
    int shmfd = shm_open(path, O_RDWR, 0666);
    if (shmfd == -1) {
        sk_error("shm_open() error: %s.", strerror(errno));
        return -1;
    }

    struct stat st;
    int ret = fstat(shmfd, &st);
    if (ret != 0) {
        // save error code as close() might change it
        int error = errno;
        close(shmfd);

        sk_error("fstat() error: %s.", strerror(error));
        errno = error;
        return -1;
    }

    if (size) *size = st.st_size;
    return shmfd;
}

int sk::detail::shm_object_resize(const char *path, size_t *size) {
    int shmfd = shm_open(path, O_RDWR, 0666);
    if (shmfd == -1) {
        sk_error("shm_open() error: %s.", strerror(errno));
        return -1;
    }

    size_t page_size = cast_size(sysconf(_SC_PAGESIZE));
    size_t real_size = *size;
    if (real_size % page_size != 0) {
        real_size += page_size - (real_size & (page_size - 1));
        sk_assert(real_size % page_size == 0);
    }

    int ret = ftruncate(shmfd, cast_s64(real_size));
    if (ret != 0) {
        // save error code as close() and shm_unlink() might change it
        int error = errno;
        close(shmfd);

        sk_error("ftruncate() error: %s.", strerror(error));
        errno = error;
        return -1;
    }

    *size = real_size;
    return shmfd;
}

void *sk::detail::shm_object_map(int shmfd, size_t *mmap_size, size_t alignment) {
    const size_t page_size = cast_size(sysconf(_SC_PAGESIZE));

    // alignment should be at least page_size due to mmap() call
    if (alignment < page_size) alignment = page_size;

    size_t real_size = *mmap_size;
    if (real_size % alignment != 0) {
        real_size += alignment - (real_size & (alignment - 1));
        sk_assert(real_size % alignment == 0);
    }

    size_t extra = 0;
    if (alignment > page_size) extra = alignment - page_size;

    void *addr = mmap(nullptr, real_size + extra, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (addr == MAP_FAILED) {
        sk_error("mmap() error: %s.", strerror(errno));
        return nullptr;
    }

    size_t skip_size = 0;
    uintptr_t ptr = reinterpret_cast<uintptr_t>(addr);
    if (ptr % alignment != 0) {
        skip_size = alignment - (ptr & (alignment - 1));
        sk_assert(skip_size <= extra);
    }

    if (skip_size <= 0) {
        // the address happens to be aligned, then we just need to release
        // the extra mapped space
        if (extra > 0) {
            int ret = munmap(reinterpret_cast<void*>(ptr + real_size), extra);
            sk_assert(ret == 0);
        }

        *mmap_size = real_size;
        return addr;
    }

    // the address is not aligned, we need to unmap the entire space, and
    // then map it at the aligned fixed address, as the fixed address and
    // size is smaller than the previous map space, so the following mmap
    // call with MAP_FIXED should always work
    int ret = munmap(addr, real_size + extra);
    sk_assert(ret == 0);

    addr = mmap(reinterpret_cast<void*>(ptr + skip_size), real_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, shmfd, 0);
    if (addr == MAP_FAILED) {
        sk_error("mmap() error: %s, previous<%p:%lu>, revised<%p:%lu>.",
                 strerror(errno),
                 reinterpret_cast<void*>(ptr), real_size + extra,
                 reinterpret_cast<void*>(ptr + skip_size), real_size);
        return nullptr;
    }

    sk_assert(addr == reinterpret_cast<void*>(ptr + skip_size));
    *mmap_size = real_size;
    return addr;
}

void sk::detail::shm_object_unmap(void *addr, size_t mmap_size) {
    int ret = munmap(addr, mmap_size);
    sk_assert(ret == 0);
}

void sk::detail::shm_object_unlink(const char *path) {
    int ret = shm_unlink(path);
    sk_assert(ret == 0);
}
