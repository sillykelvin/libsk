#include <gtest/gtest.h>
#include <iostream>
#include <libsk.h>

using namespace sk;

TEST(fixed_bitmap, normal) {
    fixed_bitmap<127> bitmap1;
    fixed_bitmap<128> bitmap2;
    fixed_bitmap<129> bitmap3;

    ASSERT_TRUE(sizeof(bitmap1) == 128 / 8);
    ASSERT_TRUE(sizeof(bitmap2) == 128 / 8);
    ASSERT_TRUE(sizeof(bitmap3) == 192 / 8);

    ASSERT_TRUE(!bitmap1.all());
    ASSERT_TRUE(!bitmap2.all());
    ASSERT_TRUE(!bitmap3.all());

    ASSERT_TRUE(!bitmap1.any());
    ASSERT_TRUE(!bitmap2.any());
    ASSERT_TRUE(!bitmap3.any());

    ASSERT_TRUE(bitmap1.none());
    ASSERT_TRUE(bitmap2.none());
    ASSERT_TRUE(bitmap3.none());

    ASSERT_TRUE(bitmap1.size() == 127);
    ASSERT_TRUE(bitmap2.size() == 128);
    ASSERT_TRUE(bitmap3.size() == 129);

    ASSERT_TRUE(bitmap1.count() == 0);
    ASSERT_TRUE(bitmap2.count() == 0);
    ASSERT_TRUE(bitmap3.count() == 0);

    for (size_t i = 0; i < 127; ++i)
        ASSERT_TRUE(!bitmap1.test(i));

    for (size_t i = 0; i < 128; ++i)
        ASSERT_TRUE(!bitmap2.test(i));

    for (size_t i = 0; i < 129; ++i)
        ASSERT_TRUE(!bitmap3.test(i));

    bitmap1.set();
    bitmap2.set();
    bitmap3.set();

    ASSERT_TRUE(bitmap1.all());
    ASSERT_TRUE(bitmap1.any());
    ASSERT_TRUE(!bitmap1.none());
    ASSERT_TRUE(bitmap1.count() == bitmap1.size());
    for (size_t i = 0; i < 127; ++i)
        ASSERT_TRUE(bitmap1.test(i));

    ASSERT_TRUE(bitmap2.all());
    ASSERT_TRUE(bitmap2.any());
    ASSERT_TRUE(!bitmap2.none());
    ASSERT_TRUE(bitmap2.count() == bitmap2.size());
    for (size_t i = 0; i < 128; ++i)
        ASSERT_TRUE(bitmap2.test(i));

    ASSERT_TRUE(bitmap3.all());
    ASSERT_TRUE(bitmap3.any());
    ASSERT_TRUE(!bitmap3.none());
    ASSERT_TRUE(bitmap3.count() == bitmap3.size());
    for (size_t i = 0; i < 129; ++i)
        ASSERT_TRUE(bitmap3.test(i));

    bitmap1.reset();
    bitmap2.reset();
    bitmap3.reset();

    ASSERT_TRUE(!bitmap1.all());
    ASSERT_TRUE(!bitmap1.any());
    ASSERT_TRUE(bitmap1.none());
    ASSERT_TRUE(bitmap1.count() == 0);

    ASSERT_TRUE(!bitmap2.all());
    ASSERT_TRUE(!bitmap2.any());
    ASSERT_TRUE(bitmap2.none());
    ASSERT_TRUE(bitmap2.count() == 0);

    ASSERT_TRUE(!bitmap3.all());
    ASSERT_TRUE(!bitmap3.any());
    ASSERT_TRUE(bitmap3.none());
    ASSERT_TRUE(bitmap3.count() == 0);

    bitmap1.set(0).set(63).set(64).set(126);
    ASSERT_TRUE(bitmap1.any());
    ASSERT_TRUE(!bitmap1.all());
    ASSERT_TRUE(!bitmap1.none());
    ASSERT_TRUE(bitmap1.test(0));
    ASSERT_TRUE(bitmap1.test(63));
    ASSERT_TRUE(bitmap1.test(64));
    ASSERT_TRUE(bitmap1.test(126));
    ASSERT_TRUE(bitmap1.count() == 4);
    ASSERT_TRUE(bitmap1.find_first() == 0);
    for (size_t i = 0; i < 127; ++i)
        if (i != 0 && i != 63 && i != 64 && i != 126)
            ASSERT_TRUE(!bitmap1.test(i));

    bitmap1.reset(0);
    ASSERT_TRUE(bitmap1.count() == 3);
    ASSERT_TRUE(bitmap1.find_first() == 63);

    bitmap1.reset(64);
    ASSERT_TRUE(bitmap1.count() == 2);
    ASSERT_TRUE(bitmap1.find_first() == 63);

    bitmap1.reset(63);
    ASSERT_TRUE(bitmap1.count() == 1);
    ASSERT_TRUE(bitmap1.find_first() == 126);
}
