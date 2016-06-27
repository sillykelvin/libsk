#include <gtest/gtest.h>
#include <iostream>
#include "libsk.h"

#define MAX_SIZE 10

using namespace sk;

struct vector_test {
    int i;

    vector_test(int i) : i(i) { std::cout << "ctor called, i: " << i << std::endl; }
    ~vector_test() { std::cout << "dtor called, i: " << i << std::endl; }

    vector_test& operator=(const vector_test& t) {
        std::cout << "op= called, this: " << this->i << ", that: " << t.i << std::endl;
        this->i = t.i;
        return *this;
    }

    bool operator==(const vector_test& t) {
        return this->i == t.i;
    }
};

struct finder {
    int i;

    finder(int i) : i(i) {}

    bool operator()(const vector_test& val) {
        return val.i + 1 == i;
    }
};

typedef fixed_vector<vector_test, MAX_SIZE> vector;

TEST(fixed_vector, normal) {
    vector ta;

    ASSERT_TRUE(ta.empty());
    ASSERT_TRUE(ta.size() == 0);
    ASSERT_TRUE(ta.capacity() == MAX_SIZE);

    for (size_t i = 0; i < ta.capacity(); ++i) {
        vector_test *t = ta.emplace(static_cast<int>(i));
        ASSERT_TRUE(t);
    }

    ASSERT_TRUE(ta.full());

    ASSERT_TRUE(ta[7].i == 7);

    vector_test value(5);
    vector_test *t = ta.find(value);
    ASSERT_TRUE(t != ta.end());
    ASSERT_TRUE(t->i == 5);

    t = ta.find_if(finder(6));
    ASSERT_TRUE(t != ta.end());
    ASSERT_TRUE(t->i == 5);

    ta.erase(value);
    ASSERT_TRUE(ta.find(value) == ta.end());

    ta.erase_if(finder(7));
    ASSERT_TRUE(ta.find(vector_test(6)) == ta.end());

    for (vector::iterator it = ta.begin(); it != ta.end(); ++it)
        std::cout << it->i << std::endl;

    t = ta.find_if(finder(10));
    ASSERT_TRUE(t != ta.end());
    ta.erase(t);

    ta.clear();
    ASSERT_TRUE(ta.empty());

    t = ta.find(value);
    ASSERT_TRUE(t == ta.end());

    ta.fill(7, vector_test(7));
    ASSERT_TRUE(ta.size() == 7);
    ASSERT_TRUE(ta[0].i == 7);
    ASSERT_TRUE(ta[3].i == 7);
    ASSERT_TRUE(ta[6].i == 7);

    ta.fill(3, vector_test(3));
    ASSERT_TRUE(ta.size() == 3);
    ASSERT_TRUE(ta[0].i == 3);
    ASSERT_TRUE(ta[1].i == 3);
    ASSERT_TRUE(ta[2].i == 3);

    ta.fill(ta.capacity(), vector_test(10));
    ASSERT_TRUE(ta.size() == ta.capacity());
    ASSERT_TRUE(ta.full());

    vector ta2(ta);
    ASSERT_TRUE(ta2.full());
    ASSERT_TRUE(ta2[0].i == 10);
    ASSERT_TRUE(ta2[9].i == 10);

    vector ta3;
    ta3 = ta;
    ASSERT_TRUE(ta3.full());
    ASSERT_TRUE(ta3[0].i == 10);
    ASSERT_TRUE(ta3[9].i == 10);
}

TEST(fixed_vector, loop_erase) {
    vector v;

    for (int i = 0; i < MAX_SIZE; ++i) {
        vector_test *t = v.emplace(i);
        ASSERT_TRUE(t);
    }

    std::vector<int> vec;
    vec.push_back(0);
    vec.push_back(2);
    vec.push_back(6);
    vec.push_back(8);
    vec.push_back(9);

    for (vector::iterator it = v.begin(); it != v.end();) {
        if (std::find(vec.begin(), vec.end(), it->i) != vec.end()) {
            v.erase(it);
        } else {
            ++it;
        }
    }

    for (int i = 0; i < MAX_SIZE; ++i) {
        vector::iterator it = v.find(vector_test(i));
        if (std::find(vec.begin(), vec.end(), i) != vec.end())
            ASSERT_TRUE(it == v.end());
        else
            ASSERT_TRUE(it != v.end());
    }
}
