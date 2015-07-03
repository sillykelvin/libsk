#include <gtest/gtest.h>
#include <iostream>
#include "sk_inc.h"

#define MAX_SIZE 5

using namespace std;
using namespace sk;

struct rarray_test {
    int i;
    size_t waste;

    rarray_test() : i(0) { cout << "ctor called" << endl; }
    ~rarray_test() { cout << "dtor called, i: " << i << endl; }
};

typedef referable_array<rarray_test, MAX_SIZE> array;

TEST(referable_array, normal) {
    array ta;

    ASSERT_TRUE(ta.empty());
    ASSERT_TRUE(ta.size() == 0);
    ASSERT_TRUE(ta.capacity() == MAX_SIZE);

    for (size_t i = 0; i < ta.capacity(); ++i) {
        rarray_test *t = ta.emplace();
        ASSERT_TRUE(t);

        t->i = i;
    }

    ASSERT_TRUE(ta.full());
    ASSERT_TRUE(ta.emplace() == NULL);

    rarray_test *t = ta.at(3);
    ASSERT_TRUE(t);
    ASSERT_TRUE(t->i == 3);
    t = ta.at(2);
    ASSERT_TRUE(t);
    ASSERT_TRUE(t->i == 2);

    ta.erase(3);
    ta.erase(2);
    ASSERT_TRUE(ta.size() == MAX_SIZE - 2);
    ASSERT_TRUE(!ta.full());
    ASSERT_TRUE(!ta.empty());
    ASSERT_TRUE(ta.at(3) == NULL);
    ASSERT_TRUE(ta.at(2) == NULL);

    size_t index;
    t = ta.emplace(&index);
    t->i = 6;
    ASSERT_TRUE(index == 2);
    ASSERT_TRUE(ta.at(index)->i == 6);
    t->i = 7;
    ASSERT_TRUE(ta.at(index)->i == 7);
    t = ta.emplace(&index);
    t->i = 8;
    ASSERT_TRUE(index == 3);
    ASSERT_TRUE(ta.at(index)->i == 8);
    ASSERT_TRUE(ta.full());

    ta.erase(1);
    ta.clear();
    ASSERT_TRUE(ta.empty());
    ta.emplace(&index)->i = 9;
    ASSERT_TRUE(index == 4);
    ASSERT_TRUE(ta.at(index));
    ASSERT_TRUE(ta.at(index)->i == 9);
}
