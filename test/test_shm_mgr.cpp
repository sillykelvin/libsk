#include <gtest/gtest.h>
#include "shm_mgr.h"

#define SHM_KEY (0x77)

TEST(shm_mgr, hash) {
    sk::detail::hash<int, int> *h = sk::detail::hash<int, int>::create(SHM_KEY, false, 10);
    ASSERT_EQ(h != NULL, true);
}
