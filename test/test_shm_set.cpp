#include <gtest/gtest.h>
#include <iostream>
#include "libsk.h"

#define SHM_MGR_KEY         (0x77777)
#define SHM_SIZE            (102400)

using namespace sk;

struct set_test {
    int i;

    explicit set_test(int i) : i(i) {}

    bool operator<(const set_test& that) const { return this->i < that.i; }
};

typedef shm_set<set_test> set;

TEST(shm_set, normal) {
    int ret = shm_mgr_init(SHM_MGR_KEY, SHM_SIZE, false);
    ASSERT_TRUE(ret == 0);

    {
        set s;

        ASSERT_TRUE(s.empty());
        ASSERT_TRUE(s.size() == 0);
        ASSERT_TRUE(s.find(set_test(1)) == s.end());

        ASSERT_TRUE(s.insert(set_test(1)) == 0);
        ASSERT_TRUE(s.insert(set_test(2)) == 0);
        ASSERT_TRUE(s.insert(set_test(3)) == 0);
        ASSERT_TRUE(s.insert(set_test(4)) == 0);
        ASSERT_TRUE(s.insert(set_test(5)) == 0);

        ASSERT_TRUE(!s.empty());
        ASSERT_TRUE(s.size() == 5);

        ASSERT_TRUE(s.find(set_test(3)) != s.end());
        ASSERT_TRUE(s.find(set_test(3))->i == 3);

        ASSERT_TRUE(s.insert(set_test(2)) == 0);
        ASSERT_TRUE(s.size() == 5);
        ASSERT_TRUE(s.insert(set_test(6)) == 0);
        ASSERT_TRUE(s.size() == 6);

        set::iterator it = s.begin();
        ++it; ++it;
        s.erase(it);
        ASSERT_TRUE(s.find(set_test(3)) == s.end());
        s.erase(set_test(2));
        ASSERT_TRUE(s.find(set_test(2)) == s.end());

        ASSERT_TRUE(s.insert(set_test(2)) == 0);
        ASSERT_TRUE(s.insert(set_test(3)) == 0);

        for (struct {int i; set::iterator it;} i = {1, s.begin()}; i.it != s.end(); ++i.it, ++i.i)
            ASSERT_TRUE(i.it->i == i.i);
    }

    shm_mgr_fini();
}
