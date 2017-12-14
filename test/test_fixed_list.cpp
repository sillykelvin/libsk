#include <gtest/gtest.h>
#include <iostream>
#include <libsk.h>

#define SHM_PATH_PREFIX "/libsk-test"
#define MAX_SIZE 5

using namespace sk;

static int ctor_call_count = 0;
static int dtor_call_count = 0;

struct list_test {
    int i;

    list_test(int i) : i(i) { ctor_call_count++; }
    ~list_test() { dtor_call_count++; }
};

typedef fixed_list<list_test, MAX_SIZE> xxlist;

TEST(fixed_list, normal) {
    int ret = shm_init(SHM_PATH_PREFIX, false);
    ASSERT_TRUE(ret == 0);

    shm_ptr<xxlist> xl = shm_new<xxlist>();
    ASSERT_TRUE(!!xl);
    xxlist& l = *xl;

    // compile error, intended
    // list::iterator xxx0(const_cast<const list&>(l).begin());
    xxlist::iterator xxx1(l.begin());
    xxlist::const_iterator xxx2(const_cast<const xxlist&>(l).begin());
    xxlist::const_iterator xxx3(l.begin());
    xxlist::const_iterator xxx4(xxx1);

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

    xxlist::iterator it = l.begin();
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

    l.pop_back();
    ASSERT_TRUE(l.emplace_back(6));

    l.pop_front();
    ASSERT_TRUE(l.emplace_front(7));

    ASSERT_TRUE(l.back()->i == 6);
    ASSERT_TRUE(l.front()->i == 7);

    std::vector<int> vec;
    vec.push_back(0);
    vec.push_back(1);
    vec.push_back(2);

    l.assign(vec.begin(), vec.end());
    ASSERT_TRUE(l.size() == 3);
    ASSERT_TRUE(l.front()->i == 0);
    l.pop_front();
    ASSERT_TRUE(l.front()->i == 1);
    l.pop_front();
    ASSERT_TRUE(l.front()->i == 2);

    l.assign(vec.begin(), vec.end());
    ASSERT_TRUE(l.size() == 3);

    l.clear();
    ASSERT_TRUE(l.empty());

    shm_delete(xl);
    ret = shm_fini();
    ASSERT_TRUE(ret == 0);
}

TEST(fixed_list, copy_assign) {
    int ret = shm_init(SHM_PATH_PREFIX, false);
    ASSERT_TRUE(ret == 0);

    shm_ptr<xxlist> xl1 = shm_new<xxlist>();
    ASSERT_TRUE(!!xl1);

    xxlist& l1 = *xl1;

    l1.emplace_back(0);
    l1.emplace_back(1);
    l1.emplace_back(2);
    ASSERT_TRUE(l1.size() == 3);

    shm_ptr<xxlist> xl2 = shm_new<xxlist>(l1);
    ASSERT_TRUE(!!xl2);

    xxlist& l2 = *xl2;

    ASSERT_TRUE(l2.size() == 3);

    xxlist::iterator it = l2.begin();
    ASSERT_TRUE(it->i == 0);
    ++it;
    ASSERT_TRUE(it->i == 1);
    ++it;
    ASSERT_TRUE(it->i == 2);

    l1.emplace_back(3);
    l1.emplace_back(4);

    l2 = l1;
    ASSERT_TRUE(l2.size() == 5);
    it = l2.end();
    --it;
    ASSERT_TRUE(it->i == 4);
    --it;
    ASSERT_TRUE(it->i == 3);
    --it;
    ASSERT_TRUE(it->i == 2);
    --it;
    ASSERT_TRUE(it->i == 1);
    --it;
    ASSERT_TRUE(it->i == 0);

    shm_delete(xl1);
    shm_delete(xl2);
    ret = shm_fini();
    ASSERT_TRUE(ret == 0);
}

TEST(fixed_list, loop_erase) {
    int ret = shm_init(SHM_PATH_PREFIX, false);
    ASSERT_TRUE(ret == 0);

    shm_ptr<fixed_list<int, MAX_SIZE>> xl = shm_new<fixed_list<int, MAX_SIZE>>();
    ASSERT_TRUE(!!xl);
    fixed_list<int, MAX_SIZE>& l = *xl;

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

    shm_delete(xl);
    ret = shm_fini();
    ASSERT_TRUE(ret == 0);
}

TEST(fixed_list, ctor_dtor) {
    int ret = shm_init(SHM_PATH_PREFIX, false);
    ASSERT_TRUE(ret == 0);

    shm_ptr<xxlist> xl = shm_new<xxlist>();
    ASSERT_TRUE(!!xl);
    xxlist& l = *xl;

    ctor_call_count = 0;
    dtor_call_count = 0;
    l.push_back(list_test(1));
    l.push_front(list_test(2));
    l.emplace_back(3);
    l.emplace_front(4);
    ASSERT_TRUE(ctor_call_count == 6);
    ASSERT_TRUE(dtor_call_count == 2);

    l.erase(l.begin());
    ASSERT_TRUE(ctor_call_count == 6);
    ASSERT_TRUE(dtor_call_count == 3);

    l.clear();
    ASSERT_TRUE(ctor_call_count == 6);
    ASSERT_TRUE(dtor_call_count == 6);

    shm_delete(xl);
    ret = shm_fini();
    ASSERT_TRUE(ret == 0);
}
