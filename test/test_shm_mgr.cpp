#include <gtest/gtest.h>
#include "libsk.h"
#include "shm/detail/shm_segment.h"

#define SHM_SEG_KEY         (0x77)
#define SHM_MGR_KEY         (0x77777)
#define SHM_SIZE            (100000)

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
    // TODO: add test for page_map here
}

TEST(shm_mgr, metadata_allocator) {
    // TODO: add test for metadata_allocator here
}

struct size24 {
    size_t a;
    size_t b;
    size_t c;
};

struct size1000 {
    char str[1000];
};

struct size1024 {
    char str1[256];
    char str2[256];
    char str3[256];
    char str4[256];
};

struct size1032 {
    char str[1024];
    size_t i;
};

struct size2064 {
    char str1[1024];
    char str2[1024];
    int a;
    int b;
    int c;
    int d;
};

TEST(shm_mgr, shm_mgr) {
    // TODO: add test here
}

TEST(shm_mgr, metadata) {
    // TODO: add test for shm_mgr::allocate_metadata(...)
}
