#ifndef SIZE_MAP_H
#define SIZE_MAP_H

#include <utility/types.h>
#include <utility/assert_helper.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

//-------------------------------------------------------------------
// Mapping from size to size_class and vice versa
//-------------------------------------------------------------------

// Sizes <= 1024 have an alignment >= 8.  So for such sizes we have an
// array indexed by ceil(size/8).  Sizes > 1024 have an alignment >= 128.
// So for these larger sizes we have an array indexed by ceil(size/128).
//
// We flatten both logical arrays into one physical array and use
// arithmetic to compute an appropriate index.  The constants used by
// class_index() were selected to make the flattening work.
//
// Examples:
//   Size       Expression                      Index
//   -------------------------------------------------------
//   0          (0 + 7) / 8                     0
//   1          (1 + 7) / 8                     1
//   ...
//   1024       (1024 + 7) / 8                  128
//   1025       (1025 + 127 + (120<<7)) / 128   129
//   ...
//   32768      (32768 + 127 + (120<<7)) / 128  376
class size_map {
public:
    static const u8 SIZE_CLASS_COUNT = 87;

    inline bool size2class(size_t bytes, u8 *sc) {
        if (likely(bytes <= MAX_SMALL_SIZE)) {
            *sc = index2class_[small_index(bytes)];
            return true;
        }

        if (likely(bytes <= MAX_SIZE)) {
            *sc = index2class_[large_index(bytes)];
            return true;
        }

        return false;
    }

    inline size_t class2size(u8 sc) const {
        assert_retval(sc < SIZE_CLASS_COUNT, 0);
        return class2size_[sc];
    }

    inline size_t class2pages(u8 sc) const {
        assert_retval(sc < SIZE_CLASS_COUNT, 0);
        return class2pages_[sc];
    }

    int init();

private:
    static const size_t MAX_SIZE = 256 * 1024;
    static const size_t MAX_SMALL_SIZE = 1024;
    static const size_t CLASS_ARRAY_SIZE = ((MAX_SIZE + 127 + (120 << 7)) >> 7) + 1;

    static const size_t BASE_ALIGNMENT = 8;
    static const size_t MIN_ALIGNMENT  = 16;

    static size_t lg_floor(size_t n);
    static size_t calc_alignment(size_t bytes);

    static inline size_t small_index(size_t bytes) {
        return (bytes + 7) >> 3;
    }

    static inline size_t large_index(size_t bytes) {
        return (bytes + 127 + (120 << 7)) >> 7;
    }

    static inline size_t class_index(size_t bytes) {
        sk_assert(bytes <= MAX_SIZE);
        if (likely(bytes <= MAX_SMALL_SIZE))
            return small_index(bytes);

        return large_index(bytes);
    }

private:
    u8 index2class_[CLASS_ARRAY_SIZE];
    size_t class2size_[SIZE_CLASS_COUNT];
    size_t class2pages_[SIZE_CLASS_COUNT];
};
static_assert(std::is_standard_layout<size_map>::value, "invalid size_map");

NS_END(detail)
NS_END(sk)

#endif // SIZE_MAP_H
