#include <gtest/gtest.h>
#include <iostream>
#include <libsk.h>

#define SHM_PATH_PREFIX "/libsk-test"

using namespace sk;

struct set_test {
    int i;

    explicit set_test(int i) : i(i) {}

    bool operator<(const set_test& that) const { return this->i < that.i; }
};

typedef shm_set<set_test> set;

TEST(shm_set, normal) {
    int ret = shm_init(SHM_PATH_PREFIX, false);
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

    shm_fini();
}

TEST(shm_set, loop_erase) {
    int ret = shm_init(SHM_PATH_PREFIX, false);
    ASSERT_TRUE(ret == 0);

    {
        set s;

        const int max_size = 20;

        for (int i = 0; i < max_size; ++i) {
            ASSERT_TRUE(s.insert(set_test(i)) == 0);
        }

        std::vector<int> vec;
        vec.push_back(1);
        vec.push_back(3);
        vec.push_back(9);
        vec.push_back(15);
        vec.push_back(19);
        for (set::iterator it = s.begin(), end = s.end(); it != end;) {
            if (std::find(vec.begin(), vec.end(), it->i) != vec.end()) {
                s.erase(it++);
            } else {
                ++it;
            }
        }

        for (int i = 0; i < max_size; ++i) {
            set::iterator it = s.find(set_test(i));
            if (std::find(vec.begin(), vec.end(), i) != vec.end())
                ASSERT_TRUE(it == s.end());
            else
                ASSERT_TRUE(it != s.end());
        }
    }

    shm_fini();
}
