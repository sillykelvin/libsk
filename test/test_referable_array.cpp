#include <gtest/gtest.h>
#include <iostream>
#include "sk_inc.h"

#define MAX_SIZE 5

using namespace sk;

struct rarray_test {
    int i;
    size_t waste;

    rarray_test(int i) : i(i) { std::cout << "ctor called, i: " << i << std::endl; }
    ~rarray_test() { std::cout << "dtor called, i: " << i << std::endl; }
};

typedef referable_array<rarray_test, MAX_SIZE> array;

TEST(referable_array, normal) {
    array ta;

    ASSERT_TRUE(ta.empty());
    ASSERT_TRUE(ta.size() == 0);
    ASSERT_TRUE(ta.capacity() == MAX_SIZE);

    pair<rarray_test*, size_t> p;
    for (size_t i = 0; i < ta.capacity(); ++i) {
        p = ta.emplace(static_cast<int>(i));
        ASSERT_TRUE(p.first);
        ASSERT_TRUE(ta.index(p.first) == p.second);
    }

    ASSERT_TRUE(ta.full());
    ASSERT_TRUE(ta.emplace(1).first == NULL);

    const array& cta = ta;
    const rarray_test *ct = cta.at(3);
    ASSERT_TRUE(ct);
    ASSERT_TRUE(ct->i == 3);

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

    p = ta.emplace(6);
    ASSERT_TRUE(p.second == 2);
    ASSERT_TRUE(ta.at(p.second)->i == 6);
    p.first->i = 7;
    ASSERT_TRUE(ta.at(p.second)->i == 7);
    p = ta.emplace(8);
    ASSERT_TRUE(p.second == 3);
    ASSERT_TRUE(ta.at(p.second)->i == 8);
    ASSERT_TRUE(ta.full());

    ta.erase(1);
    ta.clear();
    ASSERT_TRUE(ta.empty());
    p = ta.emplace(9);
    ASSERT_TRUE(p.second == 4);
    ASSERT_TRUE(ta.at(p.second));
    ASSERT_TRUE(ta.at(p.second)->i == 9);
}

struct indexed_rarray {
    int i;
    size_t next;
    size_t prev;

    indexed_rarray(int i) : i(i), next(static_cast<size_t>(-1)), prev(static_cast<size_t>(-1)) {
        std::cout << "indexed_rarray ctor called, i: " << i << std::endl;
    }
    ~indexed_rarray() { std::cout << "indexed_rarray dtor called, i: " << i << std::endl; }
};

typedef referable_array<indexed_rarray, MAX_SIZE> array2;

TEST(referable_array, copy) {
    array2 ta;

    pair<indexed_rarray*, size_t> p1;
    p1 = ta.emplace(1);
    ASSERT_TRUE(p1.first);

    pair<indexed_rarray*, size_t> p2;
    p2 = ta.emplace(2);
    ASSERT_TRUE(p2.first);

    pair<indexed_rarray*, size_t> p3;
    p3 = ta.emplace(3);
    ASSERT_TRUE(p3.first);

    p1.first->next = p2.second;
    p2.first->next = p3.second;
    p3.first->next = p1.second;

    p1.first->prev = p3.second;
    p2.first->prev = p1.second;
    p3.first->prev = p2.second;

    array2 ta2(ta);
    ASSERT_TRUE(ta2.size() == ta.size());

    indexed_rarray *t21 = ta2.at(p1.second);
    indexed_rarray *t22 = ta2.at(p2.second);
    indexed_rarray *t23 = ta2.at(p3.second);
    ASSERT_TRUE(t21 && t22 && t23);
    ASSERT_TRUE(t21->i == 1 && t22->i == 2 && t23->i == 3);
    ASSERT_TRUE(t21->next == ta2.index(t22));
    ASSERT_TRUE(t22->next == ta2.index(t23));
    ASSERT_TRUE(t23->next == ta2.index(t21));
    ASSERT_TRUE(t21->prev == ta2.index(t23));
    ASSERT_TRUE(t22->prev == ta2.index(t21));
    ASSERT_TRUE(t23->prev == ta2.index(t22));

    array2 ta3;
    ta3.emplace(999);
    ta3 = ta;
    ASSERT_TRUE(ta3.size() == ta.size());

    indexed_rarray *t31 = ta3.at(p1.second);
    indexed_rarray *t32 = ta3.at(p2.second);
    indexed_rarray *t33 = ta3.at(p3.second);
    ASSERT_TRUE(t31 && t32 && t33);
    ASSERT_TRUE(t31->i == 1 && t32->i == 2 && t33->i == 3);
    ASSERT_TRUE(t31->next == ta3.index(t32));
    ASSERT_TRUE(t32->next == ta3.index(t33));
    ASSERT_TRUE(t33->next == ta3.index(t31));
    ASSERT_TRUE(t31->prev == ta3.index(t33));
    ASSERT_TRUE(t32->prev == ta3.index(t31));
    ASSERT_TRUE(t33->prev == ta3.index(t32));
}
