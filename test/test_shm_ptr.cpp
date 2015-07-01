#include <gtest/gtest.h>
#include <iostream>
#include "sk_inc.h"

#define SHM_MGR_KEY         (0x77777)
#define HASH_SHM_KEY        (0x77)
#define STACK_SHM_KEY       (0x777)
#define BUDDY_SHM_KEY       (0x7777)
#define SHM_MGR_CHUNK_SIZE  (1000)
#define SHM_MGR_CHUNK_COUNT (1024)
#define SHM_MGR_HEAP_SIZE   (10240)

using namespace std;
using namespace sk;
using namespace sk::detail;

struct test {
    int a;
    char str[28];

    test() {
        cout << "test constructor" << endl;
        a = 7;
        snprintf(str, sizeof(str), "%s", "hello world");
    }
};

TEST(shm_ptr, shm_ptr) {
    int ret = shm_mgr_init(SHM_MGR_KEY, HASH_SHM_KEY,
                           STACK_SHM_KEY, BUDDY_SHM_KEY,
                           false, SHM_MGR_CHUNK_SIZE,
                           SHM_MGR_CHUNK_COUNT, SHM_MGR_HEAP_SIZE);
    ASSERT_TRUE(ret == 0);

    shm_ptr<test> ptr;
    ASSERT_TRUE(!ptr);

    ptr = shm_new<test>();
    ASSERT_TRUE(ptr);
    ASSERT_TRUE(ptr.mid != 0);

    ASSERT_TRUE(ptr->a == 7);
    ASSERT_STREQ(ptr->str, "hello world");
    ASSERT_TRUE((*ptr).a == 7);
    ASSERT_STREQ((*ptr).str, "hello world");

    ret = shm_mgr_fini();
    ASSERT_TRUE(ret == 0);
}
