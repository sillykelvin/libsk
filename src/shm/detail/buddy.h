#ifndef BUDDY_H
#define BUDDY_H

namespace sk {
namespace detail {

/**
 * @brief The buddy struct represents a buddy memory allocation
 * system. reference:
 *     - http://en.wikipedia.org/wiki/Buddy_memory_allocation
 *     - https://github.com/wuwenbin/buddy2
 */
struct buddy {
    u32 size;
    /*
     * NOTE: as the size stored here is always the power of 2,
     *       so we can use only one byte to store the exponent.
     *
     *       however, if we only store the exponent, we have
     *       to calculate the actual size every time we need to
     *       use.
     *
     *       so, it should be determined by usage situation: if
     *       you want to save more memory, or be faster.
     */
    u32 longest[0];

    static int __left_leaf(int index) {
        return index * 2 + 1;
    }

    static int __right_leaf(int index) {
        return index * 2 + 2;
    }

    static int __parent(int index) {
        return (index + 1) / 2 - 1;
    }

    static bool __power_of_two(u32 num) {
        return !(num & (num - 1));
    }

    static u32 __fix_size(u32 size) {
        if (__power_of_two(size))
            return size;

        size |= size >> 1;
        size |= size >> 2;
        size |= size >> 4;
        size |= size >> 8;
        size |= size >> 16;
        return size + 1;
    }

    u32 __max_child(int index) {
        u32 left  = longest[__left_leaf(index)];
        u32 right = longest[__right_leaf(index)];
        return left > right ? left : right;
    }

    static size_t calc_size(u32 size) {
        size = __fix_size(size);
        u32 leaf_count = 2 * size - 1;
        return sizeof(buddy) + (leaf_count * sizeof(u32));
    }

    static buddy *create(void *addr, size_t mem_size, bool resume, u32 size);

    int malloc(u32 size);

    void free(int offset);
};

} // namespace detail
} // namespace sk

#endif // BUDDY_H
