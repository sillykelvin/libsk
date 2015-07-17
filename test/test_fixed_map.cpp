#include <gtest/gtest.h>
#include <iostream>
#include "libsk.h"

#define MAX_SIZE 20

using namespace sk;

struct map_test {
    char c;
    int  i;

    map_test(char c, int i) : c(c), i(i) {}

    bool operator<(const map_test& that) const { return this->c < that.c; }
};

typedef fixed_map<char, map_test, MAX_SIZE> map;

TEST(fixed_map, normal) {
    map m;

    ASSERT_TRUE(m.empty());
    ASSERT_TRUE(m.size() == 0);
    ASSERT_TRUE(m.capacity() == MAX_SIZE);

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
