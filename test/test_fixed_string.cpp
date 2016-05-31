#include <gtest/gtest.h>
#include "libsk.h"

#define SMALL_SIZE 10
#define BIG_SIZE   20

using namespace sk;

typedef fixed_string<SMALL_SIZE> small_string;
typedef fixed_string<BIG_SIZE> big_string;

TEST(fixed_string, normal) {
    small_string s1;
    ASSERT_TRUE(s1.empty());
    ASSERT_TRUE(!s1.full());
    ASSERT_TRUE(s1.length() == 0);
    ASSERT_TRUE(s1.capacity() == SMALL_SIZE);
    s1 = "abcdefghijklmn";
    ASSERT_TRUE(s1.full());
    ASSERT_TRUE(s1.length() == s1.capacity() - 1);
    ASSERT_TRUE(s1 == "abcdefghi");

    small_string s2("abcde");
    ASSERT_TRUE(!s2.empty());
    ASSERT_TRUE(!s2.full());
    ASSERT_TRUE(s2.length() == 5);
    ASSERT_TRUE(s2 == "abcde");
    ASSERT_TRUE(s2 != "abcd");
    ASSERT_TRUE(s2 != "edcba");

    big_string b1(s2);
    ASSERT_TRUE(!b1.empty());
    ASSERT_TRUE(!b1.full());
    ASSERT_TRUE(b1.length() == 5);
    ASSERT_TRUE(b1 == "abcde");
    ASSERT_TRUE(b1 == s2);
    ASSERT_TRUE(b1 != "abcd");
    ASSERT_TRUE(b1 != "edcba");

    big_string b2("abcdefghijklmn");
    ASSERT_TRUE(!b2.empty());
    ASSERT_TRUE(!b2.full());
    ASSERT_TRUE(b2.length() == 14);

    small_string s3(b2);
    ASSERT_TRUE(!s3.empty());
    ASSERT_TRUE(s3.full());
    ASSERT_TRUE(s3 != b2);
    ASSERT_TRUE(s3 == "abcdefghi");

    s3.clear();
    ASSERT_TRUE(s3.empty());
    ASSERT_TRUE(s3 == "");

    big_string b3;
    b3 = s3;
    ASSERT_TRUE(b3.empty());
    ASSERT_TRUE(b3 == "");
    ASSERT_TRUE(b3 == s3);
}
