#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

#define cast_ptr(type, ptr) static_cast<type*>(static_cast<void*>(ptr))
#define void_ptr(ptr)       static_cast<void*>(ptr)
#define char_ptr(ptr)       cast_ptr(char, ptr)
#define array_len(array)    (sizeof(array) / sizeof(array[0]))

#define SHM_NULL    shm_ptr<void>()
#define IDX_NULL    (-1)
#define OFFSET_NULL 0

#define NS_BEGIN(name) namespace name {
#define NS_END(name) }

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
