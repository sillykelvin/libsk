#include <gtest/gtest.h>
#include <vector>
#include "shm_mgr.h"

#define HASH_SHM_KEY  (0x77)
#define BLK_CNT       (20)
#define STACK_SHM_KEY (0x777)
#define STACK_SIZE    (5)
#define BUDDY_SHM_KEY (0x7777)
#define BUDDY_SIZE    (60)

using namespace std;
using namespace sk::detail;

TEST(shm_mgr, hash) {
    hash<int, int> *h = hash<int, int>::create(HASH_SHM_KEY, false, 30, 10);
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

    EXPECT_TRUE(h->insert(101, 1) == 0);
    EXPECT_TRUE(h->insert(101, 2) == 0);
    EXPECT_TRUE(h->insert(101, 3) == 0);
    EXPECT_EQ(*(h->find(101)), 3);
    EXPECT_TRUE(h->erase(101, 3) == 0);
    EXPECT_EQ(*(h->find(101)), 2);
    EXPECT_TRUE(h->erase(101, 1) == 0);
    EXPECT_EQ(*(h->find(101)), 2);
    EXPECT_TRUE(h->erase(101, 2) == 0);
    EXPECT_TRUE(h->empty());

    EXPECT_TRUE(h->insert(1000, 1000) == 0);

    hash<int, int> *h2 = hash<int, int>::create(HASH_SHM_KEY, true, 30, 10);
    ASSERT_EQ(h != NULL, true);

    EXPECT_TRUE(h2->find(1000) != NULL);
    EXPECT_EQ(*(h2->find(1000)), 1000);
    EXPECT_TRUE(!h2->empty());
    EXPECT_TRUE(!h2->full());
}

TEST(shm_mgr, mem_chunk) {
    size_t mem_size = sizeof(mem_chunk) + sizeof(long) * BLK_CNT;
    mem_chunk *chunk = (mem_chunk *) malloc(mem_size);

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
        size_t offset = static_cast<char *>(static_cast<void *>(container[i])) - chunk->data;
        chunk->free(offset);
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

        size_t offset = static_cast<char *>(static_cast<void *>(*it)) - chunk->data;
        chunk->free(offset);
    }

    free(chunk);
}

TEST(shm_mgr, stack) {
    stack<int> *s = stack<int>::create(STACK_SHM_KEY, false, STACK_SIZE);
    ASSERT_EQ(s != NULL, true);

    EXPECT_TRUE(s->empty());
    EXPECT_TRUE(!s->full());

    int *i = s->emplace();
    *i = 1;

    EXPECT_TRUE(!s->empty());
    EXPECT_TRUE(!s->full());

    i = s->emplace();
    *i = 2;
    EXPECT_EQ(s->push(3), 0);

    i = s->pop();
    EXPECT_EQ(*i, 3);
    i = s->pop();
    EXPECT_EQ(*i, 2);
    i = s->pop();
    EXPECT_EQ(*i, 1);
    EXPECT_TRUE(s->pop() == NULL);

    s->push(1);
    s->push(2);
    s->push(3);
    s->push(4);
    s->push(5);
    EXPECT_TRUE(s->full());
    EXPECT_TRUE(s->emplace() == NULL);
    EXPECT_TRUE(s->push(6) == -ENOMEM);

    s->pop();
    s->pop();


    stack<int> *s2 = stack<int>::create(STACK_SHM_KEY, true, STACK_SIZE);
    ASSERT_EQ(s2 != NULL, true);

    EXPECT_TRUE(!s2->empty());
    EXPECT_TRUE(!s2->full());

    i = s2->pop();
    EXPECT_EQ(*i, 3);
    i = s2->pop();
    EXPECT_EQ(*i, 2);
    i = s2->pop();
    EXPECT_EQ(*i, 1);
    EXPECT_TRUE(s2->pop() == NULL);
}

TEST(shm_mgr, buddy) {
    buddy *b = buddy::create(BUDDY_SHM_KEY, false, BUDDY_SIZE);
    ASSERT_TRUE(b != NULL);

    int offset1 = b->malloc(4);
    ASSERT_TRUE(offset1 != -1);

    int offset2 = b->malloc(30);
    ASSERT_TRUE(offset2 != -1);

    int offset3 = b->malloc(1);
    ASSERT_TRUE(offset3 != -1);

    int offset4 = b->malloc(6);
    ASSERT_TRUE(offset4 != -1);

    int offset5 = b->malloc(10);
    ASSERT_TRUE(offset5 != -1);

    int invalid = b->malloc(3);
    ASSERT_TRUE(invalid == -1);

    b->free(offset3);

    int valid = b->malloc(3);
    ASSERT_TRUE(valid != -1);

    b->free(offset1);
    b->free(offset2);
    b->free(offset4);
    b->free(offset5);
    b->free(valid);

    valid = b->malloc(62);
    ASSERT_TRUE(valid != -1);

    b->free(valid);


    buddy *b2 = buddy::create(BUDDY_SHM_KEY, true, BUDDY_SIZE);
    ASSERT_TRUE(b2 != NULL);

    valid = b2->malloc(30);
    ASSERT_TRUE(valid != -1);
    valid = b2->malloc(27);
    ASSERT_TRUE(valid != -1);
}
