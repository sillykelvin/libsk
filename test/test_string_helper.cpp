#include <gtest/gtest.h>
#include "libsk.h"

using namespace sk;

TEST(string_helper, normal) {
    s32 value_s32 = 0;
    u32 value_u32 = 0;
    s64 value_s64 = 0;
    u64 value_u64 = 0;

    int ret = 0;

    ret = string2s32("123", value_s32);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_s32 == 123);

    ret = string2s32("-123", value_s32);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_s32 == -123);

    ret = string2s32("a1bc", value_s32);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_s32 == 0);

    ret = string2s32("1abc", value_s32);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_s32 == 1);

    ret = string2u32("123", value_u32);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_u32 == 123);

    ret = string2u32("-1", value_u32);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_u32 == (u32) -1);

    ret = string2u32("a1bc", value_u32);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_u32 == 0);

    ret = string2u32("1abc", value_u32);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_u32 == 1);

    ret = string2s64("123", value_s64);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_s64 == 123);

    ret = string2s64("-123", value_s64);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_s64 == -123);

    ret = string2s64("a1bc", value_s64);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_s64 == 0);

    ret = string2s64("1abc", value_s64);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_s64 == 1);

    ret = string2u64("123", value_u64);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_u64 == 123);

    ret = string2u64("-1", value_u64);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(value_u64 == (u64) -1);

    ret = string2u64("a1bc", value_u64);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_u64 == 0);

    ret = string2u64("1abc", value_u64);
    ASSERT_TRUE(ret != 0);
    ASSERT_TRUE(value_u64 == 1);
}
