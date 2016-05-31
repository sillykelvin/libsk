#include <gtest/gtest.h>
#include <iostream>
#include "libsk.h"

#define SHM_MGR_KEY         (0x77777)
#define SHM_SIZE            (102400)

using namespace std;
using namespace sk;
using namespace sk::detail;

struct ptr_test {
    int a;
    char str[28];

    ptr_test() {
        cout << "test constructor" << endl;
        a = 7;
        snprintf(str, sizeof(str), "%s", "hello world");
    }
};

TEST(shm_ptr, shm_ptr) {
    int ret = shm_mgr_init(SHM_MGR_KEY, SHM_SIZE, false);
    ASSERT_TRUE(ret == 0);

    shm_ptr<ptr_test> ptr;
    ASSERT_TRUE(!ptr);

    ptr = shm_new<ptr_test>();
    ASSERT_TRUE(ptr);
    ASSERT_TRUE(ptr.offset != OFFSET_NULL);

    ASSERT_TRUE(ptr->a == 7);
    ASSERT_STREQ(ptr->str, "hello world");
    ASSERT_TRUE((*ptr).a == 7);
    ASSERT_STREQ((*ptr).str, "hello world");

    shm_del(ptr);

    ret = shm_mgr_fini();
    ASSERT_TRUE(ret == 0);
}
