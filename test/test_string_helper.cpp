#include <gtest/gtest.h>
#include "libsk.h"

using namespace sk;

TEST(string_helper, normal) {
    s32 value_s32 = 0;
    u32 value_u32 = 0;
    s64 value_s64 = 0;
    u64 value_u64 = 0;

    int ret = 0;

    ret = string_traits<s32>::from_string("123", value_s32);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_s32 == 123);

    ret = string_traits<s32>::from_string("-123", value_s32);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_s32 == -123);

    ret = string_traits<s32>::from_string("a1bc", value_s32);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_s32 == 0);

    ret = string_traits<s32>::from_string("1abc", value_s32);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_s32 == 1);

    ret = string_traits<u32>::from_string("123", value_u32);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_u32 == 123);

    ret = string_traits<u32>::from_string("-1", value_u32);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_u32 == (u32) -1);

    ret = string_traits<u32>::from_string("a1bc", value_u32);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_u32 == 0);

    ret = string_traits<u32>::from_string("1abc", value_u32);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_u32 == 1);

    ret = string_traits<s64>::from_string("123", value_s64);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_s64 == 123);

    ret = string_traits<s64>::from_string("-123", value_s64);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_s64 == -123);

    ret = string_traits<s64>::from_string("a1bc", value_s64);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_s64 == 0);

    ret = string_traits<s64>::from_string("1abc", value_s64);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_s64 == 1);

    ret = string_traits<u64>::from_string("123", value_u64);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_u64 == 123);

    ret = string_traits<u64>::from_string("-1", value_u64);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_u64 == (u64) -1);

    ret = string_traits<u64>::from_string("a1bc", value_u64);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_u64 == 0);

    ret = string_traits<u64>::from_string("1abc", value_u64);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_u64 == 1);
}
