#include <gtest/gtest.h>
#include "libsk.h"

#define SHM_KEY (0x77)

TEST(shm_seg, create) {
    sk::shm_seg seg;
    int ret = seg.init(SHM_KEY, sizeof(int), false);
    ASSERT_EQ(ret, 0);

    int *i = (int *) seg.malloc(sizeof(int));
    ASSERT_EQ(i != NULL, true);
    ASSERT_EQ(seg.free_size, 0);

    *i = 7;

    int *n = (int *) seg.address();
    ASSERT_EQ(n != NULL, true);

    EXPECT_EQ(*n, 7);
}

TEST(shm_seg, resume) {
    sk::shm_seg seg;
    int ret = seg.init(SHM_KEY, sizeof(int), true);
    ASSERT_EQ(ret, 0);

    int *i = (int *) seg.malloc(sizeof(int));
    ASSERT_EQ(i == NULL, true);

    i = (int *) seg.address();

    EXPECT_EQ(*i, 7);
}

TEST(shm_seg, free) {
    sk::shm_seg seg;
    int ret = seg.init(SHM_KEY, sizeof(int), true);
    ASSERT_EQ(ret, 0);

    seg.free();

    sk::shm_seg seg2;
    ret = seg2.init(SHM_KEY, sizeof(int), true);
    EXPECT_NE(ret, 0);
}
