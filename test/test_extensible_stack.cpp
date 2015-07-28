#include <gtest/gtest.h>
#include "libsk.h"

#define STACK_SIZE (5)

using namespace sk;

TEST(extensible_stack, normal) {
    char buffer[STACK_SIZE * sizeof(int) + sizeof(extensible_stack<int>)];

    extensible_stack<int> *s = extensible_stack<int>::create(buffer, sizeof(buffer),
                                                             false, STACK_SIZE);
    ASSERT_EQ(s != NULL, true);

    EXPECT_TRUE(s->empty());
    EXPECT_TRUE(!s->full());

    int ret = s->push(1);
    EXPECT_TRUE(ret == 0);

    EXPECT_TRUE(!s->empty());
    EXPECT_TRUE(!s->full());

    ret = s->push(2);
    EXPECT_TRUE(ret == 0);
    EXPECT_EQ(s->push(3), 0);

    int *i = s->top();
    s->pop();
    EXPECT_EQ(*i, 3);
    i = s->top();
    s->pop();
    EXPECT_EQ(*i, 2);
    i = s->top();
    s->pop();
    EXPECT_EQ(*i, 1);
    EXPECT_TRUE(s->top() == NULL);

    s->push(1);
    s->push(2);
    s->push(3);
    s->push(4);
    s->push(5);
    EXPECT_TRUE(s->full());
    EXPECT_TRUE(s->push(6) == -ENOMEM);

    s->pop();
    s->pop();


    extensible_stack<int> *s2 = extensible_stack<int>::create(buffer, sizeof(buffer),
                                                              true, STACK_SIZE);
    ASSERT_EQ(s2 != NULL, true);

    EXPECT_TRUE(!s2->empty());
    EXPECT_TRUE(!s2->full());

    i = s2->top();
    s2->pop();
    EXPECT_EQ(*i, 3);
    i = s2->top();
    s2->pop();
    EXPECT_EQ(*i, 2);
    i = s2->top();
    s2->pop();
    EXPECT_EQ(*i, 1);
    EXPECT_TRUE(s2->top() == NULL);
}
