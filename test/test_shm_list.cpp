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

struct list_test {
    int a;

    list_test() {
        cout << "list test constructor" << endl;
        a = 7;
    }

    ~list_test() {
        cout << "list test destructor, a: " << a << endl;
    }
};

typedef shm_list<list_test> list;

TEST(shm_list, normal) {
    int ret = shm_mgr_init(SHM_MGR_KEY, false, SHM_MGR_CHUNK_SIZE,
                           SHM_MGR_CHUNK_COUNT, SHM_MGR_HEAP_SIZE);
    ASSERT_TRUE(ret == 0);

    shm_ptr<list> l = shm_new<list>();
    ASSERT_TRUE(l && l->empty() && l->size() == 0);
    ASSERT_TRUE(l->front() == NULL);
    ASSERT_TRUE(l->back() == NULL);
    ASSERT_TRUE(l->begin() == l->end());

    list_test t;
    list::iterator it = l->begin();

    t.a = 4;
    ret = l->insert(l->end(), t);
    ASSERT_TRUE(ret == 0);

    ASSERT_TRUE(!l->empty() && l->size() == 1);
    ASSERT_TRUE(l->front() == l->back());
    ASSERT_TRUE(l->front()->a == 4);

    t.a = 5;
    ret = l->push_back(t);
    ASSERT_TRUE(ret == 0);

    t.a = 1;
    ret = l->push_front(t);
    ASSERT_TRUE(ret == 0);

    t.a = 2;
    it = l->begin();
    ++it;
    ret = l->insert(it, t);
    ASSERT_TRUE(ret == 0);

    t.a = 3;
    ret = l->insert(it, t);
    ASSERT_TRUE(ret == 0);

    ASSERT_TRUE(l->front()->a == 1);
    ASSERT_TRUE(l->back()->a == 5);
    ASSERT_TRUE(l->size() == 5);

    it = l->begin();
    int i = 1;
    while (it != l->end()) {
        ASSERT_TRUE(it->a == i++);
        ++it;
    }

    l->clear();
    ASSERT_TRUE(l->empty() && l->size() == 0);

    l->push_front(t);

    shm_del(l);

    ret = shm_mgr_fini();
    ASSERT_TRUE(ret == 0);
}
