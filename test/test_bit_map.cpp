#include <gtest/gtest.h>
#include <iostream>
#include "libsk.h"

#define BIT_SIZE1 128
#define BIT_SIZE2 130

define_bitmap(bit_map1, BIT_SIZE1);
define_bitmap(bit_map2, BIT_SIZE2);

TEST(bit_map, normal) {
    memset(bit_map1, 0x00, sizeof(bit_map1));
    memset(bit_map2, 0x00, sizeof(bit_map2));

    ASSERT_TRUE(sizeof(bit_map1) == (128 / 8));
    ASSERT_TRUE(sizeof(bit_map2) == (192 / 8));

    ASSERT_TRUE(!test_bit(bit_map1, 10));
    set_bit(bit_map1, 10);
    ASSERT_TRUE(test_bit(bit_map1, 10));

    size_t index = find_first_bit(bit_map1, sizeof(bit_map1) * 8);
    ASSERT_TRUE(index == 10);

    clear_bit(bit_map1, 10);
    ASSERT_TRUE(!test_bit(bit_map1, 10));

    index = find_first_bit(bit_map1, sizeof(bit_map1) * 8);
    ASSERT_TRUE(index == sizeof(bit_map1) * 8);

    set_bit(bit_map1, 127);
    ASSERT_TRUE(test_bit(bit_map1, 127));

    index = find_first_bit(bit_map1, 100);
    ASSERT_TRUE(index == 100);

    set_bit(bit_map1, 0);
    index = find_first_bit(bit_map1, 128);
    ASSERT_TRUE(index == 0);

    clear_bit(bit_map1, 0);
    set_bit(bit_map1, 1);
    index = find_first_bit(bit_map1, 128);
    ASSERT_TRUE(index == 1);

    clear_bit(bit_map1, 1);
    index = find_first_bit(bit_map1, 128);
    ASSERT_TRUE(index == 127);
}
