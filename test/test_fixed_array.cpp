#include <gtest/gtest.h>
#include "sk_inc.h"

#define MAX_SIZE 10

using namespace sk;

struct finder {
    int i;

    finder(int i) : i(i) {}

    bool operator()(const int& val) {
        return val + 1 == i;
    }
};

typedef fixed_array<int, MAX_SIZE> array;

TEST(fixed_array, normal) {
    array ia;

    ASSERT_TRUE(ia.empty());
    ASSERT_TRUE(ia.size() == 0);
    ASSERT_TRUE(ia.capacity() == MAX_SIZE);

    for (size_t i = 0; i < ia.capacity(); ++i) {
        int *a = ia.emplace();
        ASSERT_TRUE(a);

        *a = i;
    }

    ASSERT_TRUE(ia.full());
    // ASSERT_DEATH(ia.emplace(), "");

    int& tmp = ia[7];
    ASSERT_TRUE(tmp == 7);

    int *a = ia.find(5);
    ASSERT_TRUE(a);
    ASSERT_TRUE(*a == 5);

    a = ia.find_if(finder(6));
    ASSERT_TRUE(a);
    ASSERT_TRUE(*a == 5);

    ia.erase(5);
    ASSERT_TRUE(ia.find(5) == NULL);

    ia.erase_if(finder(7));
    ASSERT_TRUE(ia.find(6) == NULL);

    for (array::iterator it = ia.begin(); it != ia.end(); ++it)
        printf("%d\n", *it);

    ia.clear();
    ASSERT_TRUE(ia.empty());

    a = ia.find(5);
    ASSERT_TRUE(!a);
}
