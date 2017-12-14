#include <gtest/gtest.h>
#include <iostream>
#include <libsk.h>

#define SHM_PATH_PREFIX "/libsk-test"
#define MAX_SIZE 20

using namespace sk;

struct fixed_map_test {
    char c;
    int  i;

    fixed_map_test() : c(0), i(0) {}

    fixed_map_test(char c, int i) : c(c), i(i) {}

    bool operator<(const fixed_map_test& that) const { return this->c < that.c; }
};

typedef fixed_map<char, fixed_map_test, MAX_SIZE> map;

TEST(fixed_map, normal) {
    int ret = shm_init(SHM_PATH_PREFIX, false);
    ASSERT_TRUE(ret == 0);

    shm_ptr<map> xm = shm_new<map>();
    ASSERT_TRUE(!!xm);
    map& m = *xm;

    ASSERT_TRUE(m.empty());
    ASSERT_TRUE(m.size() == 0);
    ASSERT_TRUE(m.capacity() == MAX_SIZE);

    ret = m.insert(make_pair('a', fixed_map_test('a', 'a')));
    ASSERT_TRUE(ret == 0);
    ret = m.insert(make_pair('b', fixed_map_test('b', 'b')));
    ASSERT_TRUE(ret == 0);
    ret = m.insert(make_pair('c', fixed_map_test('c', 'c')));
    ASSERT_TRUE(ret == 0);

    ASSERT_TRUE(!m.empty());
    ASSERT_TRUE(m.size() == 3);

    map::iterator it = m.find('a');
    ASSERT_TRUE(it != m.end());
    ASSERT_TRUE(it->first == 'a' && it->second.c == 'a' && it->second.i == 'a');

    m.erase(it);
    ASSERT_TRUE(m.find('a') == m.end());

    ret = m.insert(make_pair('a', fixed_map_test('a', 'a')));
    ASSERT_TRUE(ret == 0);
    it = m.find('a');
    it->second.c = 'h';
    it->second.i = 'h';
    ASSERT_TRUE(m.find('a')->second.c == 'h');
    ASSERT_TRUE(m.find('a')->second.i == 'h');

    it = m.begin();
    // it->first = 'x'; // should not compile, intended
    ++it;
    it->second.c = 'i';
    it->second.i = 'i';
    ++it;
    it->second.c = 'j';
    it->second.i = 'j';

    map::const_iterator cit = it;
    // cit->first    = 'x'; // should not compile, intended
    // cit->second.c = 'x';
    // cit->second.i = 'x';

    for (struct {char c; map::iterator it;} i = {'a', m.begin()}; i.it != m.end(); ++i.it, ++i.c) {
        ASSERT_TRUE(i.it->first == i.c);
        ASSERT_TRUE(i.it->second.c == i.c + 'h' - 'a');
        ASSERT_TRUE(i.it->second.i == i.c + 'h' - 'a');
    }

    m.clear();
    for (int i = 0; i < MAX_SIZE; ++i) {
        char c = 'a' + i;
        ASSERT_TRUE(m.emplace(c, fixed_map_test(c, c)));
        ASSERT_TRUE(m.find(c) != m.end());
    }

    for (struct {char c; map::const_reverse_iterator it;} i = {'a' + MAX_SIZE - 1, m.rbegin()}; i.it != m.rend(); ++i.it, --i.c) {
        ASSERT_TRUE(i.it->first == i.c);
        ASSERT_TRUE(i.it->second.c == i.c);
        ASSERT_TRUE(i.it->second.i == i.c);
    }

    shm_delete(xm);
    ret = shm_fini();
    ASSERT_TRUE(ret == 0);
}

TEST(fixed_map, loop_erase) {
    int ret = shm_init(SHM_PATH_PREFIX, false);
    ASSERT_TRUE(ret == 0);

    shm_ptr<fixed_map<int, int, MAX_SIZE>> xm = shm_new<fixed_map<int, int, MAX_SIZE>>();
    ASSERT_TRUE(!!xm);
    fixed_map<int, int, MAX_SIZE>& m = *xm;

    for (int i = 0; i < MAX_SIZE; ++i) {
        ret = m.insert(make_pair(i, i));
        ASSERT_TRUE(ret == 0);
    }
    ASSERT_TRUE(m.full());
    ret = m.insert(make_pair(99, 99));
    ASSERT_TRUE(ret != 0);

    std::vector<int> vec;
    vec.push_back(1);
    vec.push_back(3);
    vec.push_back(9);
    vec.push_back(15);
    vec.push_back(19);
    for (fixed_map<int, int, MAX_SIZE>::iterator it = m.begin(), end = m.end(); it != end;) {
        if (std::find(vec.begin(), vec.end(), it->first) != vec.end()) {
            m.erase(it++);
        } else {
            ++it;
        }
    }

    for (int i = 0; i < MAX_SIZE; ++i) {
        fixed_map<int, int, MAX_SIZE>::iterator it = m.find(i);
        if (std::find(vec.begin(), vec.end(), i) != vec.end())
            ASSERT_TRUE(it == m.end());
        else
            ASSERT_TRUE(it != m.end());
    }

    shm_delete(xm);
    ret = shm_fini();
    ASSERT_TRUE(ret == 0);
}
