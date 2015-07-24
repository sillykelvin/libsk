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
