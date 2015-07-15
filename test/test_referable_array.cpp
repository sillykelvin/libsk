#include <gtest/gtest.h>
#include <iostream>
#include "sk_inc.h"

#define MAX_SIZE 5

using namespace sk;

struct rarray_test {
    int i;
    size_t waste;

    rarray_test() : i(0) { std::cout << "ctor called" << std::endl; }
    ~rarray_test() { std::cout << "dtor called, i: " << i << std::endl; }
};

typedef referable_array<rarray_test, MAX_SIZE> array;

TEST(referable_array, normal) {
    array ta;

    ASSERT_TRUE(ta.empty());
    ASSERT_TRUE(ta.size() == 0);
    ASSERT_TRUE(ta.capacity() == MAX_SIZE);

    for (size_t i = 0; i < ta.capacity(); ++i) {
        size_t index = array::npos;
        rarray_test *t = ta.emplace(&index);
        ASSERT_TRUE(t);
        ASSERT_TRUE(ta.index(t) == index);

        t->i = i;
    }

    ASSERT_TRUE(ta.full());
    ASSERT_TRUE(ta.emplace() == NULL);

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

struct indexed_rarray {
    int i;
    size_t next;
    size_t prev;

    indexed_rarray() : i(0), next(static_cast<size_t>(-1)), prev(static_cast<size_t>(-1)) {
        std::cout << "indexed_rarray ctor called" << std::endl;
    }
    ~indexed_rarray() { std::cout << "indexed_rarray dtor called, i: " << i << std::endl; }
};

typedef referable_array<indexed_rarray, MAX_SIZE> array2;

TEST(referable_array, copy) {
    array2 ta;

    size_t idx1 = array2::npos;
    indexed_rarray *t1 = ta.emplace(&idx1);
    ASSERT_TRUE(t1);
    t1->i = 1;

    size_t idx2 = array2::npos;
    indexed_rarray *t2 = ta.emplace(&idx2);
    ASSERT_TRUE(t2);
    t2->i = 2;

    size_t idx3 = array2::npos;
    indexed_rarray *t3 = ta.emplace(&idx3);
    ASSERT_TRUE(t3);
    t3->i = 3;

    t1->next = idx2;
    t2->next = idx3;
    t3->next = idx1;

    t1->prev = idx3;
    t2->prev = idx1;
    t3->prev = idx2;

    array2 ta2(ta);
    ASSERT_TRUE(ta2.size() == ta.size());

    indexed_rarray *t21 = ta2.at(idx1);
    indexed_rarray *t22 = ta2.at(idx2);
    indexed_rarray *t23 = ta2.at(idx3);
    ASSERT_TRUE(t21 && t22 && t23);
    ASSERT_TRUE(t21->i == 1 && t22->i == 2 && t23->i == 3);
    ASSERT_TRUE(t21->next == ta2.index(t22));
    ASSERT_TRUE(t22->next == ta2.index(t23));
    ASSERT_TRUE(t23->next == ta2.index(t21));
    ASSERT_TRUE(t21->prev == ta2.index(t23));
    ASSERT_TRUE(t22->prev == ta2.index(t21));
    ASSERT_TRUE(t23->prev == ta2.index(t22));

    array2 ta3;
    ta3.emplace()->i = 999;
    ta3 = ta;
    ASSERT_TRUE(ta3.size() == ta.size());

    indexed_rarray *t31 = ta3.at(idx1);
    indexed_rarray *t32 = ta3.at(idx2);
    indexed_rarray *t33 = ta3.at(idx3);
    ASSERT_TRUE(t31 && t32 && t33);
    ASSERT_TRUE(t31->i == 1 && t32->i == 2 && t33->i == 3);
    ASSERT_TRUE(t31->next == ta3.index(t32));
    ASSERT_TRUE(t32->next == ta3.index(t33));
    ASSERT_TRUE(t33->next == ta3.index(t31));
    ASSERT_TRUE(t31->prev == ta3.index(t33));
    ASSERT_TRUE(t32->prev == ta3.index(t31));
    ASSERT_TRUE(t33->prev == ta3.index(t32));
}
