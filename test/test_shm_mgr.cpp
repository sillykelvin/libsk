#include <gtest/gtest.h>
#include "libsk.h"
#include "shm/detail/shm_segment.h"
#include "shm/detail/page_map.h"
#include "shm/detail/metadata_allocator.h"

#define SHM_SEG_KEY         (0x77)
#define SHM_MGR_KEY         (0x77777)
#define SHM_SIZE            (102400)

using namespace sk;
using namespace sk::detail;

TEST(shm_mgr, shm_segment) {
    {
        sk::detail::shm_segment seg;
        int ret = seg.init(SHM_SEG_KEY, sizeof(int), false);
        ASSERT_TRUE(ret == 0);

        int *n = cast_ptr(int, seg.address());
        ASSERT_TRUE(n != NULL);

        *n = 77;

        seg.release();
    }

    {
        sk::detail::shm_segment seg;
        int ret = seg.init(SHM_SEG_KEY, sizeof(int), true);
        ASSERT_TRUE(ret == 0);

        int *n = cast_ptr(int, seg.address());
        ASSERT_TRUE(n != NULL);
        ASSERT_TRUE(*n == 77);
    }

    {
        sk::detail::shm_segment seg;
        int ret = seg.init(SHM_SEG_KEY, sizeof(int), true);
        ASSERT_TRUE(ret != 0);
    }
}

TEST(shm_mgr, page_map) {
    page_map m;
    m.init();

    int ret = shm_mgr_init(SHM_MGR_KEY, SHM_SIZE, false);
    ASSERT_TRUE(ret == 0);

    offset_ptr<void> tmp(777);

    ASSERT_TRUE(m.get(0) == offset_ptr<void>::null());
    m.set(0, tmp);
    ASSERT_TRUE(m.get(0) == tmp);

    ASSERT_TRUE(m.get(1) == offset_ptr<void>::null());
    m.set(1, tmp);
    ASSERT_TRUE(m.get(1) == tmp);

    ASSERT_TRUE(m.get(222) == offset_ptr<void>::null());
    m.set(222, tmp);
    ASSERT_TRUE(m.get(222) == tmp);

    ASSERT_TRUE(m.get(3333) == offset_ptr<void>::null());
    m.set(3333, tmp);
    ASSERT_TRUE(m.get(3333) == tmp);

    ASSERT_TRUE(m.get(99999) == offset_ptr<void>::null());
    m.set(99999, tmp);
    ASSERT_TRUE(m.get(99999) == tmp);

    shm_mgr_fini();
}

TEST(shm_mgr, metadata_allocator) {
    metadata_allocator<size_t> allocator;
    allocator.init();

    int ret = shm_mgr_init(SHM_MGR_KEY, SHM_SIZE, false);
    ASSERT_TRUE(ret == 0);

    offset_ptr<size_t> ptr = allocator.allocate();
    ASSERT_TRUE(ptr != offset_ptr<size_t>::null());
    offset_t offset = ptr.offset;

    allocator.deallocate(ptr);
    ptr = allocator.allocate();
    ASSERT_TRUE(ptr != offset_ptr<size_t>::null());
    ASSERT_TRUE(ptr.offset == offset);

    allocator.deallocate(ptr);
    ASSERT_TRUE(allocator.stat.alloc_count == 2);
    ASSERT_TRUE(allocator.stat.free_count == 2);

    shm_mgr_fini();
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
