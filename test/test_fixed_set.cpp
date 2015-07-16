#include <gtest/gtest.h>
#include <iostream>
#include "sk_inc.h"

#define MAX_SIZE 20

using namespace sk;

struct set_test {
    int i;

    set_test() {}
    explicit set_test(int i) : i(i) {}

    bool operator<(const set_test& that) const { return this->i < that.i; }
};

typedef fixed_set<set_test, MAX_SIZE> set;

TEST(fixed_set, normal) {
    set s;

    ASSERT_TRUE(s.empty());
    ASSERT_TRUE(!s.full());
    ASSERT_TRUE(s.size() == 0);
    ASSERT_TRUE(s.capacity() == MAX_SIZE);
    ASSERT_TRUE(s.find(set_test(1)) == s.end());

    ASSERT_TRUE(s.insert(set_test(1)) == 0);
    ASSERT_TRUE(s.insert(set_test(2)) == 0);
    ASSERT_TRUE(s.insert(set_test(3)) == 0);
    ASSERT_TRUE(s.insert(set_test(4)) == 0);
    ASSERT_TRUE(s.insert(set_test(5)) == 0);

    ASSERT_TRUE(!s.empty() && !s.full());
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
