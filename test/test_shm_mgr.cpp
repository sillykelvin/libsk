#include <gtest/gtest.h>
#include <vector>
#include "shm_mgr.h"

#define SHM_KEY (0x77)
#define BLK_CNT (20)

using namespace std;
using namespace sk::detail;

TEST(shm_mgr, hash) {
    hash<int, int> *h = hash<int, int>::create(SHM_KEY, false, 10);
    ASSERT_EQ(h != NULL, true);
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
