#include <gtest/gtest.h>
#include <shm/detail/shm_object.h>
#include <shm/detail/metadata_allocator.h>
#include <libsk.h>

#define SHM_PATH        "/libsk-test.mmap"
#define SHM_PATH_PREFIX "/libsk-test"

using namespace sk;
using namespace sk::detail;

TEST(shm_mgr, shm_object) {
    int shmfd = -1;
    size_t page_size = cast_size(sysconf(_SC_PAGESIZE));
    size_t real_size = 1;
    size_t mmap_size = page_size * 2;

    shmfd = shm_object_create(SHM_PATH, &real_size);
    ASSERT_TRUE(shmfd != -1);
    ASSERT_TRUE(real_size == page_size);

    void *addr = shm_object_map(shmfd, &mmap_size, page_size);
    ASSERT_TRUE(addr);
    ASSERT_TRUE(mmap_size == page_size * 2);

    close(shmfd);
    memset(addr, 0x00, page_size);

    int *array = cast_ptr(int, addr);
    const int array_len = (int) (page_size / sizeof(int));
    for (int i = 0; i < array_len; ++i)
        array[i] = 77 + i;

    real_size = page_size + page_size - 1;
    shmfd = shm_object_resize(SHM_PATH, &real_size);
    ASSERT_TRUE(shmfd != -1);
    ASSERT_TRUE(real_size == page_size * 2);

    close(shmfd);
    for (int i = 0; i < array_len; ++i)
        ASSERT_TRUE(array[i] == 77 + i);

    real_size = 0;
    shm_object_unmap(addr, mmap_size);
    shmfd = shm_object_attach(SHM_PATH, &real_size);
    ASSERT_TRUE(shmfd != -1);
    ASSERT_TRUE(real_size == page_size * 2);

    addr = shm_object_map(shmfd, &mmap_size, page_size);
    ASSERT_TRUE(addr);
    ASSERT_TRUE(mmap_size == page_size * 2);

    close(shmfd);
    array = cast_ptr(int, addr);
    for (int i = 0; i < array_len; ++i)
        ASSERT_TRUE(array[i] == 77 + i);

    shmfd = shm_object_create(SHM_PATH, &real_size);
    ASSERT_TRUE(shmfd == -1);

    shm_object_unmap(addr, mmap_size);
    shm_object_unlink(SHM_PATH);

    shmfd = shm_object_attach(SHM_PATH, &real_size);
    ASSERT_TRUE(shmfd == -1);
}

TEST(shm_mgr, metadata_allocator) {
    metadata_allocator<size_t> allocator;

    int ret = shm_init(SHM_PATH_PREFIX, false);
    ASSERT_TRUE(ret == 0);

    shm_address addr = allocator.allocate();
    ASSERT_TRUE(!!addr);

    shm_offset_t offset = addr.offset();
    allocator.deallocate(addr);
    addr = allocator.allocate();
    ASSERT_TRUE(!!addr);
    ASSERT_TRUE(addr.offset() == offset);

    allocator.deallocate(addr);
    shm_fini();
}

TEST(shm_mgr, size_map) {
    // TODO: add test here
}

TEST(shm_mgr, span) {
    // TODO: add test here
}

TEST(shm_mgr, page_heap) {
    // TODO: add test here
}

TEST(shm_mgr, chunk_cache) {
    // TODO: add test here
}

TEST(shm_mgr, shm_mgr) {
    // TODO: add test here
}
