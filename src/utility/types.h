#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

/*
 * sometimes, we need a magic code to identify something
 * "code feed" :-)
 */
#define SK_MAGIC 0xC0DEFEED

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define unused_parameter(x) (void)(x)

#define cast_ptr(type, ptr) static_cast<type*>(static_cast<void*>(ptr))
#define void_ptr(ptr)       static_cast<void*>(ptr)
#define char_ptr(ptr)       cast_ptr(char, ptr)

#define cast_value(type, value) static_cast<type>(value)
#define cast_u64(value) cast_value(u64, (value))
#define cast_s64(value) cast_value(s64, (value))
#define cast_u32(value) cast_value(u32, (value))
#define cast_s32(value) cast_value(s32, (value))
#define cast_u16(value) cast_value(u16, (value))
#define cast_s16(value) cast_value(s16, (value))
#define cast_u8(value)  cast_value(u8,  (value))
#define cast_s8(value)  cast_value(s8,  (value))
#define cast_int(value) cast_value(int, (value))
#define cast_size(value) cast_value(size_t, (value))

#define array_len(array)    (sizeof(array) / sizeof(array[0]))

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

static_assert(sizeof(s8)  == 1, "inconsistent name and type");
static_assert(sizeof(u8)  == 1, "inconsistent name and type");
static_assert(sizeof(s16) == 2, "inconsistent name and type");
static_assert(sizeof(u16) == 2, "inconsistent name and type");
static_assert(sizeof(s32) == 4, "inconsistent name and type");
static_assert(sizeof(u32) == 4, "inconsistent name and type");
static_assert(sizeof(s64) == 8, "inconsistent name and type");
static_assert(sizeof(u64) == 8, "inconsistent name and type");

#endif // TYPES_H
