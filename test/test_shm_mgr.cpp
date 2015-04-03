#include <gtest/gtest.h>
#include <vector>
#include "shm_mgr.h"

#define SHM_KEY (0x77)
#define BLK_CNT (20)

using namespace std;
using namespace sk::detail;

TEST(shm_mgr, hash) {
    hash<int, int> *h = hash<int, int>::create(SHM_KEY, false, 30, 10);
    ASSERT_EQ(h != NULL, true);

    EXPECT_TRUE(h->empty());

    int *v = h->find(1);
    EXPECT_TRUE(v == NULL);

    int ret = h->insert(0, 0);
    EXPECT_TRUE(ret == 0);

    ret = h->insert(10, 10);
    EXPECT_TRUE(ret == 0);

    EXPECT_EQ(*(h->find(0)), 0);
    EXPECT_EQ(*(h->find(10)), 10);

    ret = h->erase(0);
    EXPECT_TRUE(ret == 0);

    ret = h->erase(10);
    EXPECT_TRUE(ret == 0);

    for (int i = 0; i < 30; ++i) {
        ASSERT_TRUE(h->insert(i, i) == 0);
    }

    ASSERT_TRUE(h->full());

    EXPECT_TRUE(h->insert(100, 100) != 0);

    for (int i = 29; i >= 0; --i) {
        EXPECT_TRUE(*h->find(i) == i);
    }

    for (int i = 0; i < 30; ++i) {
        EXPECT_TRUE(h->erase(i) == 0);
    }

    EXPECT_TRUE(h->empty());

    EXPECT_TRUE(h->insert(101, 1) == 0);
    EXPECT_TRUE(h->insert(101, 2) == 0);

    EXPECT_TRUE(h->find(101) != NULL);
    EXPECT_TRUE(h->erase(101) == 0);
    EXPECT_TRUE(h->find(101) != NULL);
    EXPECT_TRUE(h->erase(101) == 0);
    EXPECT_TRUE(h->find(101) == NULL);
    EXPECT_TRUE(h->empty());

    EXPECT_TRUE(h->insert(1000, 1000) == 0);

    hash<int, int> *h2 = hash<int, int>::create(SHM_KEY, true, 30, 10);
    ASSERT_EQ(h != NULL, true);

    EXPECT_TRUE(h2->find(1000) != NULL);
    EXPECT_EQ(*(h2->find(1000)), 1000);
    EXPECT_TRUE(!h2->empty());
    EXPECT_TRUE(!h2->full());
}

TEST(shm_mgr, small_chunk) {
    size_t mem_size = sizeof(small_chunk) + sizeof(long) * BLK_CNT;
    small_chunk *chunk = (small_chunk *) malloc(mem_size);
    new (chunk) small_chunk();

    ASSERT_EQ(chunk != NULL, true);

    int ret = chunk->init(mem_size, sizeof(long));
    ASSERT_EQ(ret, 0);

    ASSERT_EQ(chunk->full(), false);

    std::vector<long *> container;

    for (int i = 0; i < BLK_CNT; ++i) {
        long *l = (long *) chunk->malloc();
        ASSERT_EQ(l != NULL, true);

        *l = i;
        container.push_back(l);
    }

    ASSERT_EQ(chunk->full(), true);

    for (int i = 0; i < 10; ++i) {
        chunk->free(container[i]);
        container.erase(container.begin() + i);
    }

    for (int i = 100; i < 110; ++i) {
        long *l = (long *) chunk->malloc();
        ASSERT_EQ(l != NULL, true);

        *l = i;
        container.push_back(l);
    }

    ASSERT_EQ(chunk->full(), true);

    for (std::vector<long *>::iterator it = container.begin();
         it != container.end(); ++it) {
        cout << *(*it) << endl;
        chunk->free(*it);
    }

    free(chunk);
}
