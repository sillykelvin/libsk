#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>
#include <algorithm>

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define cast_ptr(type, ptr) static_cast<type*>(static_cast<void*>(ptr))
#define void_ptr(ptr)       static_cast<void*>(ptr)
#define char_ptr(ptr)       cast_ptr(char, ptr)
#define array_len(array)    (sizeof(array) / sizeof(array[0]))

#define div_round_up(n, d) (((n) + (d) - 1) / (d))
#define bits2u64(n) div_round_up((n), 8 * sizeof(u64))
#define define_bitmap(name, bits) u64 name[bits2u64(bits)]

#define set_bit(addr, index) *(cast_ptr(u32, (addr)) + ((index) >> 5)) |= (1 << ((index) & 31))
#define clear_bit(addr, index) *(cast_ptr(u32, (addr)) + ((index) >> 5)) &= ~(1 << ((index) & 31))
#define test_bit(addr, index)  !!(1 & (*(cast_ptr(u32, (addr)) + ((index) >> 5)) >> ((index) & 31)))

/**
 * @brief find_first_bit - find the first set bit in a memory region
 * @param addr: the address to start the search at
 * @param size: the maximum number of BITS to search
 * @return the bit number of the first set bit, if no bits are set,
 *         returns @size
 */
inline size_t find_first_bit(const uint64_t *addr, size_t size) {
    static const size_t bits_per_u64 = sizeof(uint64_t) * 8;
    for (size_t i = 0; (i * bits_per_u64) < size; ++i)
        if (addr[i]) {
            uint64_t value = addr[i];
            // return std::min((i * bits_per_u64) + __builtin_ctzl(addr[i]), size);
            return std::min((i * bits_per_u64) + __builtin_popcountl((value - 1) & ~value), size);
        }

    return size;
}

#define SHM_NULL    sk::shm_ptr<void>()
#define IDX_NULL    (-1)
#define MID_NULL    0
#define OFFSET_NULL 0

#define NS_BEGIN(name) namespace name {
#define NS_END(name) }

#define MAKE_NONCOPYABLE(T) T(const T&) = delete; T& operator=(const T&) = delete

typedef int8_t   s8;
typedef uint8_t  u8;
typedef int16_t  s16;
typedef uint16_t u16;
typedef int32_t  s32;
typedef uint32_t u32;
typedef int64_t  s64;
typedef uint64_t u64;

typedef size_t offset_t;
typedef size_t page_t;

static_assert(sizeof(s8)  == 1, "inconsistent name and type");
static_assert(sizeof(u8)  == 1, "inconsistent name and type");
static_assert(sizeof(s16) == 2, "inconsistent name and type");
static_assert(sizeof(u16) == 2, "inconsistent name and type");
static_assert(sizeof(s32) == 4, "inconsistent name and type");
static_assert(sizeof(u32) == 4, "inconsistent name and type");
static_assert(sizeof(s64) == 8, "inconsistent name and type");
static_assert(sizeof(u64) == 8, "inconsistent name and type");

#endif // TYPES_H
