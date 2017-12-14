#include <gtest/gtest.h>
#include <iostream>
#include <libsk.h>

#define SHM_PATH_PREFIX "/libsk-test"

using namespace sk;

static int ctor_call_count = 0;
static int dtor_call_count = 0;

struct shm_map_test {
    char c;
    int  i;

    shm_map_test(char c, int i) : c(c), i(i) { ctor_call_count++; }
    shm_map_test(const shm_map_test& m) : c(m.c), i(m.i) { ctor_call_count++; }
    ~shm_map_test() { dtor_call_count++; }

    bool operator<(const shm_map_test& that) const { return this->c < that.c; }
};

typedef shm_map<char, shm_map_test> map;

TEST(shm_map, normal) {
    int ret = shm_init(SHM_PATH_PREFIX, false);
    ASSERT_TRUE(ret == 0);

    {
        shm_ptr<map> xm = shm_new<map>();
        ASSERT_TRUE(!!xm);
        map& m = *xm;

        ASSERT_TRUE(m.empty());
        ASSERT_TRUE(m.size() == 0);

        int ret = 0;
        ret = m.insert(make_pair('a', shm_map_test('a', 'a')));
        ASSERT_TRUE(ret == 0);
        ret = m.insert(make_pair('b', shm_map_test('b', 'b')));
        ASSERT_TRUE(ret == 0);
        ret = m.insert(make_pair('c', shm_map_test('c', 'c')));
        ASSERT_TRUE(ret == 0);

        ASSERT_TRUE(!m.empty());
        ASSERT_TRUE(m.size() == 3);

        map::iterator it = m.find('a');
        ASSERT_TRUE(it != m.end());
        ASSERT_TRUE(it->first == 'a' && it->second.c == 'a' && it->second.i == 'a');

        m.erase(it);
        ASSERT_TRUE(m.find('a') == m.end());

        ret = m.insert(make_pair('a', shm_map_test('a', 'a')));
        ASSERT_TRUE(ret == 0);
        it = m.find('a');
        it->second.c = 'h';
        it->second.i = 'h';
        ASSERT_TRUE(m.find('a')->second.c == 'h');
        ASSERT_TRUE(m.find('a')->second.i == 'h');

        it = m.begin();
        ++it;
        it->second.c = 'i';
        it->second.i = 'i';
        ++it;
        it->second.c = 'j';
        it->second.i = 'j';

        for (struct {char c; map::iterator it;} i = {'a', m.begin()}; i.it != m.end(); ++i.it, ++i.c) {
            ASSERT_TRUE(i.it->first == i.c);
            ASSERT_TRUE(i.it->second.c == i.c + 'h' - 'a');
            ASSERT_TRUE(i.it->second.i == i.c + 'h' - 'a');
        }

        shm_delete(xm);
    }

    ret = shm_fini();
    ASSERT_TRUE(ret == 0);
}

TEST(shm_map, ctor_dtor) {
    int ret = shm_init(SHM_PATH_PREFIX, false);
    ASSERT_TRUE(ret == 0);

    {
        shm_ptr<map> xm = shm_new<map>();
        ASSERT_TRUE(!!xm);
        map& m = *xm;

        ctor_call_count = 0;
        dtor_call_count = 0;

        m.insert(make_pair('a', shm_map_test('a', 'a')));
        m.insert(make_pair('b', shm_map_test('b', 'b')));
        m.insert(make_pair('c', shm_map_test('c', 'c')));
        ASSERT_TRUE(m.size() == 3);
        ASSERT_TRUE(ctor_call_count == 12);
        ASSERT_TRUE(dtor_call_count == 9);

        m.emplace('d', shm_map_test('d', 'd'));
        m.emplace('e', shm_map_test('e', 'e'));
        m.emplace('f', shm_map_test('f', 'f'));
        ASSERT_TRUE(m.size() == 6);
        ASSERT_TRUE(ctor_call_count == 21);
        ASSERT_TRUE(dtor_call_count == 15);

        m.erase(m.begin());
        m.erase(m.begin());
        ASSERT_TRUE(m.size() == 4);
        ASSERT_TRUE(ctor_call_count == 21);
        ASSERT_TRUE(dtor_call_count == 17);

        m.clear();
        ASSERT_TRUE(m.size() == 0);
        ASSERT_TRUE(ctor_call_count == 21);
        ASSERT_TRUE(dtor_call_count == 21);

        shm_delete(xm);
    }

    ret = shm_fini();
    ASSERT_TRUE(ret == 0);
}

TEST(shm_map, loop_erase) {
    int ret = shm_init(SHM_PATH_PREFIX, false);
    ASSERT_TRUE(ret == 0);

    {
        shm_ptr<shm_map<int, int>> xm = shm_new<shm_map<int, int>>();
        ASSERT_TRUE(!!xm);
        shm_map<int, int>& m = *xm;

        int ret = 0;
        for (int i = 0; i < 20; ++i) {
            ret = m.insert(make_pair(i, i));
            ASSERT_TRUE(ret == 0);
        }

        std::vector<int> vec;
        vec.push_back(1);
        vec.push_back(3);
        vec.push_back(9);
        vec.push_back(15);
        vec.push_back(19);
        for (shm_map<int, int>::iterator it = m.begin(), end = m.end(); it != end;) {
            if (std::find(vec.begin(), vec.end(), it->first) != vec.end()) {
                m.erase(it++);
            } else {
                ++it;
            }
        }

        for (int i = 0; i < 20; ++i) {
            shm_map<int, int>::iterator it = m.find(i);
            if (std::find(vec.begin(), vec.end(), i) != vec.end())
                ASSERT_TRUE(it == m.end());
            else
                ASSERT_TRUE(it != m.end());
        }

        shm_delete(xm);
    }

    ret = shm_fini();
    ASSERT_TRUE(ret == 0);
}
