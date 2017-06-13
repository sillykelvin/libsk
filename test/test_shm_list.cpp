#include <gtest/gtest.h>
#include <iostream>
#include "libsk.h"

#define SHM_MGR_KEY         (0x77777)
#define SHM_SIZE            (1024000)

using namespace sk;
using namespace sk::detail;

struct list_test {
    int a;

    list_test() {
        std::cout << "list test constructor" << std::endl;
        a = 7;
    }

    ~list_test() {
        std::cout << "list test destructor, a: " << a << std::endl;
    }

    bool operator==(const list_test& that) const {
        return this->a == that.a;
    }
};

typedef shm_list<list_test> list;

TEST(shm_list, normal) {
    int ret = shm_mgr_init(SHM_MGR_KEY, SHM_SIZE, false);
    ASSERT_TRUE(ret == 0);

    shm_ptr<list> l = shm_new<list>();
    ASSERT_TRUE(l && l->empty() && l->size() == 0);
    ASSERT_TRUE(l->front() == NULL);
    ASSERT_TRUE(l->back() == NULL);
    ASSERT_TRUE(l->begin() == l->end());

    list_test t;
    list::iterator it = l->begin();

    t.a = 4;
    ret = l->insert(l->end(), t);
    ASSERT_TRUE(ret == 0);

    ASSERT_TRUE(!l->empty() && l->size() == 1);
    ASSERT_TRUE(l->front() == l->back());
    ASSERT_TRUE(l->front()->a == 4);

    t.a = 5;
    ret = l->push_back(t);
    ASSERT_TRUE(ret == 0);

    t.a = 1;
    ret = l->push_front(t);
    ASSERT_TRUE(ret == 0);

    t.a = 2;
    it = l->begin();
    ++it;
    ret = l->insert(it, t);
    ASSERT_TRUE(ret == 0);

    t.a = 3;
    ret = l->insert(it, t);
    ASSERT_TRUE(ret == 0);

    ASSERT_TRUE(l->front()->a == 1);
    ASSERT_TRUE(l->back()->a == 5);
    ASSERT_TRUE(l->size() == 5);

    it = l->begin();
    int i = 1;
    while (it != l->end()) {
        ASSERT_TRUE(it->a == i++);
        ++it;
    }

    it = l->begin();
    l->erase(it);
    ASSERT_TRUE(l->size() == 4);
    ASSERT_TRUE(l->front()->a == 2);

    it = l->begin();
    ++it;
    ++it;
    ++it;
    l->erase(it);
    ASSERT_TRUE(l->size() == 3);
    ASSERT_TRUE(l->back()->a == 4);

    it = l->begin();
    ++it;
    l->erase(it--);
    ASSERT_TRUE(l->size() == 2);
    ASSERT_TRUE(l->front()->a == 2);
    it++;
    l->erase(--it);
    ASSERT_TRUE(l->size() == 1);
    ASSERT_TRUE(l->front()->a == 4);
    l->erase(l->begin());
    ASSERT_TRUE(l->empty() && l->size() == 0);
    ASSERT_TRUE(l->front() == NULL && l->back() == NULL);

    t.a = 999;
    l->push_front(t);
    ASSERT_TRUE(!l->empty());

    l->clear();
    ASSERT_TRUE(l->empty() && l->size() == 0);

    t.a = 111;
    l->push_front(t);

    shm_del(l);

    ret = shm_mgr_fini();
    ASSERT_TRUE(ret == 0);
}

TEST(shm_list, loop_erase) {
    int ret = shm_mgr_init(SHM_MGR_KEY, SHM_SIZE, false);
    ASSERT_TRUE(ret == 0);

    shm_ptr<list> l = shm_new<list>();
    ASSERT_TRUE(!!l);

    const int max_size = 20;

    for (int i = 0; i < max_size; ++i) {
        list_test t;
        t.a = i;
        ASSERT_TRUE(l->push_back(t) == 0);
    }

    std::vector<int> vec;
    vec.push_back(1);
    vec.push_back(3);
    vec.push_back(9);
    vec.push_back(15);
    vec.push_back(19);
    for (list::iterator it = l->begin(), end = l->end(); it != end;) {
        if (std::find(vec.begin(), vec.end(), it->a) != vec.end()) {
            l->erase(it++);
        } else {
            ++it;
        }
    }

    for (int i = 0; i < max_size; ++i) {
        list_test t;
        t.a = i;
        list::iterator it = std::find(l->begin(), l->end(), t);
        if (std::find(vec.begin(), vec.end(), i) != vec.end())
            ASSERT_TRUE(it == l->end());
        else
            ASSERT_TRUE(it != l->end());
    }

    shm_del(l);

    ret = shm_mgr_fini();
    ASSERT_TRUE(ret == 0);
}
