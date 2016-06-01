#include <gtest/gtest.h>
#include <iostream>
#include "libsk.h"

#define SHM_MGR_KEY         (0x77777)
#define SHM_SIZE            (102400)

using namespace sk;

struct map_test {
    char c;
    int  i;

    map_test(char c, int i) : c(c), i(i) {}

    bool operator<(const map_test& that) const { return this->c < that.c; }
};

typedef shm_map<char, map_test> map;

TEST(shm_map, normal) {
    int ret = shm_mgr_init(SHM_MGR_KEY, SHM_SIZE, false);
    ASSERT_TRUE(ret == 0);

    {
        map m;

        ASSERT_TRUE(m.empty());
        ASSERT_TRUE(m.size() == 0);

        int ret = 0;
        ret = m.insert('a', map_test('a', 'a'));
        ASSERT_TRUE(ret == 0);
        ret = m.insert('b', map_test('b', 'b'));
        ASSERT_TRUE(ret == 0);
        ret = m.insert('c', map_test('c', 'c'));
        ASSERT_TRUE(ret == 0);

        ASSERT_TRUE(!m.empty());
        ASSERT_TRUE(m.size() == 3);

        map::iterator it = m.find('a');
        ASSERT_TRUE(it != m.end());
        ASSERT_TRUE(it->first == 'a' && it->second.c == 'a' && it->second.i == 'a');

        m.erase(it);
        ASSERT_TRUE(m.find('a') == m.end());

        ret = m.insert('a', map_test('a', 'a'));
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
    }

    shm_mgr_fini();
}

TEST(shm_map, loop_erase) {
    int ret = shm_mgr_init(SHM_MGR_KEY, SHM_SIZE, false);
    ASSERT_TRUE(ret == 0);

    {
        shm_map<int, int> m;

        int ret = 0;
        for (int i = 0; i < 20; ++i) {
            ret = m.insert(i, i);
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
    }

    shm_mgr_fini();
}
