#include <gtest/gtest.h>
#include <iostream>
#include "sk_inc.h"

#define MAX_SIZE 3

using namespace std;
using namespace sk;

struct stack_test {
    int i;

    stack_test() : i(0) { cout << "ctor called" << endl; }
    ~stack_test() { cout << "dtor called, i: " << i << endl; }
};

typedef fixed_stack<stack_test, MAX_SIZE> stack;

TEST(fixed_stack, normal) {
    stack ta;

    ASSERT_TRUE(ta.empty());
    ASSERT_TRUE(ta.size() == 0);
    ASSERT_TRUE(ta.capacity() == MAX_SIZE);

    for (size_t i = 0; i < ta.capacity(); ++i) {
        stack_test *t = ta.emplace();
        ASSERT_TRUE(t);

        t->i = i;
    }

    ASSERT_TRUE(ta.full());

    stack_test *t = ta.top();
    ASSERT_TRUE(t->i == 2);
    ta.pop();

    t = ta.top();
    ASSERT_TRUE(t->i == 1);
    ta.pop();

    ta.pop();
    ASSERT_TRUE(ta.empty());
}
