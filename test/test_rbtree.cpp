#include <gtest/gtest.h>
#include <iostream>
#include "sk_inc.h"

#define MAX_SIZE 20

using namespace std;
using namespace sk;

struct rbtree_test {
    int i;

    rbtree_test() {}
    explicit rbtree_test(int i) : i(i) { /*cout << "ctor called, i: " << i << endl;*/ }
    ~rbtree_test() { /*cout << "dtor called, i: " << i << endl;*/ }

    bool operator<(const rbtree_test& that) const {
        return this->i < that.i;
    }
};

typedef fixed_rbtree<rbtree_test, rbtree_test, MAX_SIZE> tree;

TEST(fixed_rbtree, normal) {
    tree ta;

    ASSERT_TRUE(ta.empty());
    ASSERT_TRUE(ta.size() == 0);
    ASSERT_TRUE(ta.capacity() == MAX_SIZE);
    ASSERT_TRUE(ta.__check());
    ASSERT_TRUE(!ta.get(rbtree_test(1)));

    for (size_t i = 1; i <= ta.capacity(); ++i) {
        int ret = ta.put(rbtree_test(i), rbtree_test(i));
        ASSERT_TRUE(ret == 0);
        ASSERT_TRUE(ta.get(rbtree_test(i))->i == static_cast<int>(i));
        ASSERT_TRUE(ta.contains(rbtree_test(i)));
        ASSERT_TRUE(ta.size() == i);
        ASSERT_TRUE(ta.__check());
    }

    ASSERT_TRUE(ta.full());
    ASSERT_TRUE(ta.contains(rbtree_test(7)));
    ta.erase(rbtree_test(7));
    ASSERT_TRUE(!ta.contains(rbtree_test(7)));
    ASSERT_TRUE(!ta.full());
    ASSERT_TRUE(ta.__check());

    int values[] = { 2, 9, 5, 18, 6, 10, 11, 3 };
    for (size_t i = 0; i < sizeof(values) / sizeof(int); ++i) {
        ASSERT_TRUE(ta.contains(rbtree_test(values[i])));
        ta.erase(rbtree_test(values[i]));
        ASSERT_TRUE(!ta.contains(rbtree_test(values[i])));
        ASSERT_TRUE(!ta.full());
        ASSERT_TRUE(ta.__check());
    }

    for (size_t i = 0; i < sizeof(values) / sizeof(int); ++i) {
        ASSERT_TRUE(!ta.contains(rbtree_test(i)));
        int ret = ta.put(rbtree_test(values[i]), rbtree_test(values[i]));
        ASSERT_TRUE(ret == 0);
        ASSERT_TRUE(ta.contains(rbtree_test(i)));
        ASSERT_TRUE(!ta.full());
        ASSERT_TRUE(ta.__check());
    }

    rbtree_test *min = ta.min();
    ASSERT_TRUE(min && min->i == 1);

    rbtree_test *tmp = ta.floor(rbtree_test(7));
    ASSERT_TRUE(tmp && tmp->i == 6);
    tmp = ta.floor(rbtree_test(12));
    ASSERT_TRUE(tmp && tmp->i == 12);
    tmp = ta.floor(rbtree_test(0));
    ASSERT_TRUE(!tmp);

    tmp = ta.ceiling(rbtree_test(7));
    ASSERT_TRUE(tmp && tmp->i == 8);
    tmp = ta.ceiling(rbtree_test(15));
    ASSERT_TRUE(tmp && tmp->i == 15);
    tmp = ta.ceiling(rbtree_test(22));
    ASSERT_TRUE(!tmp);

    ASSERT_TRUE(ta.__check());
}
