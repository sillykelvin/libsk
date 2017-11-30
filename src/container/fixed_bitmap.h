#ifndef FIXED_BITMAP_H
#define FIXED_BITMAP_H

#include <utility/types.h>
#include <utility/assert_helper.h>

NS_BEGIN(sk)

template<size_t N>
struct fixed_bitmap {
    // N == 0 will make "WORD_COUNT - 1" overflow, so we
    // disable fixed_bitmap<0> here, as it makes nonsense
    static_assert(N >= 1, "bit count 0 is disabled");

    typedef u64 word_type;

    static const size_t WORD_BITS  = 8 * sizeof(word_type);
    static const size_t WORD_MASK  = WORD_BITS - 1;
    static const size_t WORD_SHIFT = 6;
    static const size_t WORD_COUNT = (N + WORD_BITS - 1) / WORD_BITS;
    static_assert(1 << WORD_SHIFT == WORD_BITS, "incorrect word shift");

    static const size_t EXTRA_BITS = N & WORD_MASK;
    static const size_t WASTE_BITS = (EXTRA_BITS == 0) ? 0 : WORD_BITS - EXTRA_BITS;

    static const word_type WORD_ONE  =  static_cast<word_type>(1);
    static const word_type WORD_ZERO =  static_cast<word_type>(0);
    static const word_type WORD_FULL = ~static_cast<word_type>(0);

    word_type words[WORD_COUNT];

    fixed_bitmap() { reset(); }

    size_t size() const { return N; }

    size_t count() const {
        size_t n = 0;
        for (size_t i = 0; i < WORD_COUNT; ++i)
            n += __builtin_popcountl(words[i]);

        return n;
    }

    bool all() const {
        for (size_t i = 0; i < WORD_COUNT - 1; ++i) {
            if (words[i] != WORD_FULL)
                return false;
        }

        return words[WORD_COUNT - 1] == (WORD_FULL >> WASTE_BITS);
    }

    bool any() const {
        for (size_t i = 0; i < WORD_COUNT; ++i) {
            if (words[i] != WORD_ZERO)
                return true;
        }

        return false;
    }

    bool none() const {
        return !any();
    }

    bool test(size_t pos) const {
        assert_retval(pos < N, false);

        return (words[pos >> WORD_SHIFT] & (WORD_ONE << (pos & WORD_MASK))) != 0;
    }

    fixed_bitmap<N>& set() {
        for (size_t i = 0; i < WORD_COUNT; ++i)
            words[i] = WORD_FULL;

        if (EXTRA_BITS != 0)
            words[WORD_COUNT - 1] &= ~(WORD_FULL << EXTRA_BITS);

        return *this;
    }

    fixed_bitmap<N>& set(size_t pos) {
        assert_retval(pos < N, *this);

        words[pos >> WORD_SHIFT] |= (WORD_ONE << (pos & WORD_MASK));
        return *this;
    }

    fixed_bitmap<N>& reset() {
        memset(words, 0x00, sizeof(words));
        return *this;
    }

    fixed_bitmap<N>& reset(size_t pos) {
        assert_retval(pos < N, *this);

        words[pos >> WORD_SHIFT] &= ~(WORD_ONE << (pos & WORD_MASK));
        return *this;
    }

    fixed_bitmap<N>& flip() {
        for (size_t i = 0; i < WORD_COUNT; ++i)
            words[i] = ~words[i];

        words[WORD_COUNT - 1] &= ~(WORD_FULL << EXTRA_BITS);
        return *this;
    }

    fixed_bitmap<N>& flip(size_t pos) {
        assert_retval(pos < N, *this);

        words[pos >> WORD_SHIFT] ^= (WORD_ONE << (pos & WORD_MASK));
        return *this;
    }

    size_t find_first() const {
        for (size_t i = 0; i < WORD_COUNT; ++i) {
            if (words[i] != WORD_ZERO)
                return i * WORD_BITS + __builtin_ctzl(words[i]);
        }

        return N;
    }
};

NS_END(sk)

#endif // FIXED_BITMAP_H
