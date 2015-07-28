#include <gtest/gtest.h>
#include "libsk.h"

using namespace sk;

TEST(extensible_hash, normal) {
    const size_t mem_size = extensible_hash<int, int>::calc_size(30);
    char buffer[mem_size];
    extensible_hash<int, int> *h = extensible_hash<int, int>::create(buffer, sizeof(buffer), false, 30);
    ASSERT_TRUE(h != NULL);

    EXPECT_TRUE(h->empty());

    int *v = h->find(1);
    EXPECT_TRUE(v == NULL);

    int ret = h->insert(0, 0);
    EXPECT_TRUE(ret == 0);

    ret = h->insert(10, 10);
    EXPECT_TRUE(ret == 0);

    EXPECT_EQ(*(h->find(0)), 0);
    EXPECT_EQ(*(h->find(10)), 10);

    h->erase(0);
    h->erase(10);

    for (int i = 0; i < 30; ++i) {
        ASSERT_TRUE(h->insert(i, i) == 0);
    }

    ASSERT_TRUE(h->full());

    EXPECT_TRUE(h->insert(100, 100) != 0);

    for (int i = 29; i >= 0; --i) {
        EXPECT_TRUE(*h->find(i) == i);
    }

    for (int i = 0; i < 30; ++i) {
        h->erase(i);
    }

    EXPECT_TRUE(h->empty());

    EXPECT_TRUE(h->insert(101, 1) == 0);
    EXPECT_TRUE(h->insert(101, 2) == 0);

    EXPECT_TRUE(h->find(101) != NULL);
    h->erase(101);
    EXPECT_TRUE(h->find(101) != NULL);
    h->erase(101);
    EXPECT_TRUE(h->find(101) == NULL);
    EXPECT_TRUE(h->empty());

    EXPECT_TRUE(h->insert(101, 1) == 0);
    EXPECT_TRUE(h->insert(101, 2) == 0);
    EXPECT_TRUE(h->insert(101, 3) == 0);
    EXPECT_EQ(*(h->find(101)), 3);
    h->erase(101, 3);
    EXPECT_EQ(*(h->find(101)), 2);
    h->erase(101, 1);
    EXPECT_EQ(*(h->find(101)), 2);
    h->erase(101, 2);
    EXPECT_TRUE(h->empty());

    EXPECT_TRUE(h->insert(1000, 1000) == 0);

    extensible_hash<int, int> *h2 = extensible_hash<int, int>::create(buffer, sizeof(buffer), true, 30);
    ASSERT_TRUE(h != NULL);

    EXPECT_TRUE(h2->find(1000) != NULL);
    EXPECT_EQ(*(h2->find(1000)), 1000);
    EXPECT_TRUE(!h2->empty());
    EXPECT_TRUE(!h2->full());
}
