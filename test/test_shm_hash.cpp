#include <gtest/gtest.h>
#include <iostream>
#include "libsk.h"

#define SHM_MGR_KEY         (0x77777)
#define SHM_MGR_CHUNK_SIZE  (10240)
#define SHM_MGR_CHUNK_COUNT (1024)
#define SHM_MGR_HEAP_SIZE   (102400)
#define SHM_HASH_NODE_COUNT (10)

using namespace std;
using namespace sk;
using namespace sk::detail;

struct hash_test {
    int a;
    char str[28];

    hash_test() {
        cout << "hash test constructor" << endl;
        a = 7;
        snprintf(str, sizeof(str), "%s", "hello world");
    }

    ~hash_test() {
        cout << "hash test destructor, a: " << a << endl;
    }
};

typedef shm_hash<u64, hash_test, hashfunc> hash;

TEST(shm_hash, normal) {
    int ret = shm_mgr_init(SHM_MGR_KEY, false, SHM_MGR_CHUNK_SIZE,
                           SHM_MGR_CHUNK_COUNT, SHM_MGR_HEAP_SIZE);
    ASSERT_TRUE(ret == 0);

    shm_ptr<hash> h = shm_new<hash>(SHM_HASH_NODE_COUNT);
    ASSERT_TRUE(h->empty());

    hash_test t;

    t.a = 1;
    ret = h->insert(1, t);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(!h->empty());

    t.a = 14;
    ret = h->insert(14, t);
    ASSERT_TRUE(ret == 0);

    hash_test *p = h->find(1);
    ASSERT_TRUE(p);
    ASSERT_TRUE(p->a == 1);

    p = h->find(2);
    ASSERT_TRUE(!p);

    t.a = 111;
    ret = h->insert(1, t);
    ASSERT_TRUE(ret == 0);

    p = h->find(1);
    ASSERT_TRUE(p);
    ASSERT_TRUE(p->a == 111);

    h->erase(1);
    p = h->find(1);
    ASSERT_TRUE(!p);

    h->erase(14);
    ASSERT_TRUE(h->empty());

    for (size_t i = 0; i < SHM_HASH_NODE_COUNT; ++i) {
        t.a = int(i);
        ret = h->insert(i, t);
        ASSERT_TRUE(ret == 0);
    }
    ASSERT_TRUE(h->full());

    ASSERT_TRUE(h->insert(99, t) != 0);

    h->clear();
    ASSERT_TRUE(h->empty());

    ASSERT_TRUE(h->insert(99, t) == 0);

    shm_del(h);

    ret = shm_mgr_fini();
    ASSERT_TRUE(ret == 0);
}
