#include <gtest/gtest.h>
#include <iostream>
#include "sk_inc.h"

#define MAX_SIZE 10

using namespace sk;

struct array_test {
    int i;

    array_test(int i) : i(i) { std::cout << "ctor called, i: " << i << std::endl; }
    ~array_test() { std::cout << "dtor called, i: " << i << std::endl; }

    array_test& operator=(const array_test& t) {
        std::cout << "op= called, this: " << this->i << ", that: " << t.i << std::endl;
        this->i = t.i;
        return *this;
    }

    bool operator==(const array_test& t) {
        return this->i == t.i;
    }
};

struct finder {
    int i;

    finder(int i) : i(i) {}

    bool operator()(const array_test& val) {
        return val.i + 1 == i;
    }
};

typedef fixed_array<array_test, MAX_SIZE> array;

TEST(fixed_array, normal) {
    array ta;

    ASSERT_TRUE(ta.empty());
    ASSERT_TRUE(ta.size() == 0);
    ASSERT_TRUE(ta.capacity() == MAX_SIZE);

    for (size_t i = 0; i < ta.capacity(); ++i) {
        array_test *t = ta.emplace(static_cast<int>(i));
        ASSERT_TRUE(t);
    }

    ASSERT_TRUE(ta.full());

    ASSERT_TRUE(ta[7].i == 7);

    array_test value(5);
    array_test *t = ta.find(value);
    ASSERT_TRUE(t);
    ASSERT_TRUE(t->i == 5);

    t = ta.find_if(finder(6));
    ASSERT_TRUE(t);
    ASSERT_TRUE(t->i == 5);

    ta.erase(value);
    ASSERT_TRUE(ta.find(value) == NULL);

    ta.erase_if(finder(7));
    ASSERT_TRUE(ta.find(array_test(6)) == NULL);

    for (array::iterator it = ta.begin(); it != ta.end(); ++it)
        std::cout << it->i << std::endl;

    t = ta.find_if(finder(10));
    ASSERT_TRUE(t);
    ta.erase(t);

    ta.clear();
    ASSERT_TRUE(ta.empty());

    t = ta.find(value);
    ASSERT_TRUE(!t);

    ta.fill(7, array_test(7));
    ASSERT_TRUE(ta.size() == 7);
    ASSERT_TRUE(ta[0].i == 7);
    ASSERT_TRUE(ta[3].i == 7);
    ASSERT_TRUE(ta[6].i == 7);

    ta.fill(3, array_test(3));
    ASSERT_TRUE(ta.size() == 3);
    ASSERT_TRUE(ta[0].i == 3);
    ASSERT_TRUE(ta[1].i == 3);
    ASSERT_TRUE(ta[2].i == 3);

    ta.fill(ta.capacity(), array_test(10));
    ASSERT_TRUE(ta.size() == ta.capacity());
    ASSERT_TRUE(ta.full());

    array ta2(ta);
    ASSERT_TRUE(ta2.full());
    ASSERT_TRUE(ta2[0].i == 10);
    ASSERT_TRUE(ta2[9].i == 10);

    array ta3;
    ta3 = ta;
    ASSERT_TRUE(ta3.full());
    ASSERT_TRUE(ta3[0].i == 10);
    ASSERT_TRUE(ta3[9].i == 10);
}
