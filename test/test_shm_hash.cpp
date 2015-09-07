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

    hash_test() {
        cout << "hash test constructor" << endl;
        a = 7;
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
    ASSERT_TRUE(h && h->empty());

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

TEST(shm_hash, iterator) {
    int ret = shm_mgr_init(SHM_MGR_KEY, false, SHM_MGR_CHUNK_SIZE,
                           SHM_MGR_CHUNK_COUNT, SHM_MGR_HEAP_SIZE);
    ASSERT_TRUE(ret == 0);

    shm_ptr<hash> h = shm_new<hash>(SHM_HASH_NODE_COUNT);
    ASSERT_TRUE(h && h->empty());

    hash::iterator it = h->begin();
    ASSERT_TRUE(it.n == NULL && it == h->end());

    hash_test t;

    t.a = 1;
    ret = h->insert(1, t);
    ASSERT_TRUE(ret == 0);
    t.a = 14;
    ret = h->insert(14, t);
    ASSERT_TRUE(ret == 0);

    t.a = 2;
    ret = h->insert(2, t);
    ASSERT_TRUE(ret == 0);

    t.a = 4;
    ret = h->insert(4, t);
    ASSERT_TRUE(ret == 0);

    t.a = 7;
    ret = h->insert(7, t);
    ASSERT_TRUE(ret == 0);

    t.a = 9;
    ret = h->insert(9, t);
    ASSERT_TRUE(ret == 0);
    t.a = 22;
    ret = h->insert(22, t);
    ASSERT_TRUE(ret == 0);
    t.a = 35;
    ret = h->insert(35, t);
    ASSERT_TRUE(ret == 0);
    t.a = 48;
    ret = h->insert(48, t);
    ASSERT_TRUE(ret == 0);

    t.a = 12;
    ret = h->insert(12, t);
    ASSERT_TRUE(ret == 0);

    ASSERT_TRUE(h->full());

    it = h->begin();
    ASSERT_TRUE(it->first == 14 && it->second.a == 14);
    ++it;
    ASSERT_TRUE(it->first == 1 && it->second.a == 1);
    ++it;
    ASSERT_TRUE(it->first == 2 && it->second.a == 2);
    it++;
    ASSERT_TRUE(it->first == 4 && it->second.a == 4);
    ++it;
    ASSERT_TRUE(it->first == 7 && it->second.a == 7);
    it++;
    ASSERT_TRUE(it->first == 48 && it->second.a == 48);
    ++it;
    ASSERT_TRUE(it->first == 35 && it->second.a == 35);
    ++it;
    ASSERT_TRUE(it->first == 22 && it->second.a == 22);
    it++;
    ASSERT_TRUE(it->first == 9 && it->second.a == 9);
    ++it;
    ASSERT_TRUE(it->first == 12 && it->second.a == 12);

    hash::const_iterator cit = it;
    // cit->second.a = 999;   // this should cannot compile

    it++;
    ASSERT_TRUE(it.n == NULL && it == h->end());

    ASSERT_TRUE(cit->first == 12 && cit->second.a == 12);
    --cit;
    ASSERT_TRUE(cit->first == 9 && cit->second.a == 9);
    cit--;
    ASSERT_TRUE(cit->first == 22 && cit->second.a == 22);
    --cit;
    ASSERT_TRUE(cit->first == 35 && cit->second.a == 35);
    cit--;
    ASSERT_TRUE(cit->first == 48 && cit->second.a == 48);
    --cit;
    ASSERT_TRUE(cit->first == 7 && cit->second.a == 7);
    --cit;
    ASSERT_TRUE(cit->first == 4 && cit->second.a == 4);
    cit--;
    ASSERT_TRUE(cit->first == 2 && cit->second.a == 2);
    --cit;
    ASSERT_TRUE(cit->first == 1 && cit->second.a == 1);
    cit--;
    ASSERT_TRUE(cit->first == 14 && cit->second.a == 14);

    it = h->begin();
    for (; it != h->end(); ++it)
        cout << "key: " << it->first << ", value: " << it->second.a << endl;

    shm_del(h);

    ret = shm_mgr_fini();
    ASSERT_TRUE(ret == 0);
}
