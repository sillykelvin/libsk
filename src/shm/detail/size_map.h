#ifndef SIZE_MAP_H
#define SIZE_MAP_H

#include "utility/types.h"
#include "utility/config.h"
#include "utility/assert_helper.h"

namespace sk {
namespace detail {

struct size_map {
    static const size_t MAX_SMALL_SIZE = 1024;

    u8 index2class[((MAX_SIZE + 127) >> 7) + 120 + 1];
    size_t class2size[SIZE_CLASS_COUNT];
    int class2pages[SIZE_CLASS_COUNT];

    static inline u32 __small_index(size_t bytes) {
        return (static_cast<u32>(bytes) + 7) >> 3;
    }

    static inline u32 __large_index(size_t bytes) {
        return ((static_cast<u32>(bytes) + 127) >> 7) + 120;
    }

    static inline u32 __index(size_t bytes) {
        if (bytes <= MAX_SMALL_SIZE)
            return __small_index(bytes);

        return __large_index(bytes);
    }

    inline int size_class(size_t bytes) {
        u32 idx = __index(bytes);
        assert_retval(idx < array_len(index2class), -1);

        return index2class[idx];
    }

    inline size_t class_to_size(int sc) const {
        assert_retval(sc >= 0 && sc < SIZE_CLASS_COUNT, 0);
        return class2size[sc];
    }

    inline int class_to_pages(int sc) const {
        assert_retval(sc >= 0 && sc < SIZE_CLASS_COUNT, 0);
        return class2pages[sc];
    }

    int init();
};

} // namespace detail
} // namespace sk

#endif // SIZE_MAP_H
