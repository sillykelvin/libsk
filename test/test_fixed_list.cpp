#include <gtest/gtest.h>
#include <iostream>
#include "libsk.h"

#define MAX_SIZE 5

using namespace sk;

struct list_test {
    int i;

    list_test(int i) : i(i) {}
};

typedef fixed_list<list_test, MAX_SIZE> list;

TEST(fixed_list, normal) {
    list l;

    ASSERT_TRUE(l.empty());
    ASSERT_TRUE(l.size() == 0);
    ASSERT_TRUE(l.capacity() == MAX_SIZE);
    ASSERT_TRUE(!l.front());
    ASSERT_TRUE(!l.back());

    ASSERT_TRUE(l.insert(l.end(), list_test(3)) == 0);
    ASSERT_TRUE(l.begin()->i == 3);
    ASSERT_TRUE(l.insert(l.begin(), list_test(1)) == 0);
    ASSERT_TRUE(l.begin()->i == 1);
    ASSERT_TRUE(l.insert((++l.begin()), list_test(2)) == 0);
    ASSERT_TRUE(l.insert(l.end(), list_test(4)) == 0);
    ASSERT_TRUE(l.insert(l.end(), list_test(5)) == 0);

    ASSERT_TRUE(l.full());
    ASSERT_TRUE(l.insert(l.end(), list_test(999)) != 0);

    list::iterator it = l.begin();
    ASSERT_TRUE(it != l.end());
    ASSERT_TRUE(it->i == 1);
    ++it;
    ASSERT_TRUE(it->i == 2);
    it++;
    ASSERT_TRUE(it->i == 3);
    ++it;
    ASSERT_TRUE(it->i == 4);
    ++it;
    ASSERT_TRUE(it->i == 5);
    --it;
    ASSERT_TRUE(it->i == 4);
    it--;
    ASSERT_TRUE(it->i == 3);


    ASSERT_TRUE(l.front()->i == 1);
    ASSERT_TRUE(l.back()->i == 5);

    l.erase(l.begin()++);
    l.erase(l.begin());
    l.pop_front();
    ASSERT_TRUE(l.begin()->i == 4);
    ASSERT_TRUE(l.push_front(list_test(3)) == 0);
    l.pop_back();
    ASSERT_TRUE(l.back()->i == 4);
    ASSERT_TRUE(l.push_back(list_test(5)) == 0);

    l.clear();
    ASSERT_TRUE(l.empty());
}

TEST(fixed_list, loop_erase) {
    fixed_list<int, MAX_SIZE> l;

    int ret = 0;
    for (int i = 0; i < MAX_SIZE; ++i) {
        ret = l.push_back(i);
        ASSERT_TRUE(ret == 0);
    }
    ASSERT_TRUE(l.full());

    std::vector<int> vec;
    vec.push_back(0);
    vec.push_back(2);
    vec.push_back(4);

    for (fixed_list<int, MAX_SIZE>::iterator it = l.begin(), end = l.end(); it != end;) {
        if (std::find(vec.begin(), vec.end(), *it) != vec.end())
            it = l.erase(it);
        else
            ++it;
    }

    for (int i = 0; i < MAX_SIZE; ++i) {
        fixed_list<int, MAX_SIZE>::iterator it = std::find(l.begin(), l.end(), i);
        if (std::find(vec.begin(), vec.end(), i) != vec.end())
            ASSERT_TRUE(it == l.end());
        else
            ASSERT_TRUE(it != l.end());
    }

    l.clear();
    ASSERT_TRUE(l.empty());
    for (int i = 0; i < MAX_SIZE; ++i) {
        ret = l.push_back(i);
        ASSERT_TRUE(ret == 0);
    }
    ASSERT_TRUE(l.full());

    for (fixed_list<int, MAX_SIZE>::iterator it = l.begin(), end = l.end(); it != end;) {
        if (std::find(vec.begin(), vec.end(), *it) != vec.end())
            l.erase(it++);
        else
            ++it;
    }

    for (int i = 0; i < MAX_SIZE; ++i) {
        fixed_list<int, MAX_SIZE>::iterator it = std::find(l.begin(), l.end(), i);
        if (std::find(vec.begin(), vec.end(), i) != vec.end())
            ASSERT_TRUE(it == l.end());
        else
            ASSERT_TRUE(it != l.end());
    }
}
