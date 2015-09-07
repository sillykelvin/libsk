#ifndef TYPES_H
#define TYPES_H

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

#define cast_ptr(type, ptr) static_cast<type*>(static_cast<void*>(ptr))
#define void_ptr(ptr)       static_cast<void*>(ptr)
#define char_ptr(ptr)       cast_ptr(char, ptr)

#define SHM_NULL shm_ptr<void>()

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

static const size_t INVALID_OFFSET = (size_t) 0;
static const int IDX_NULL = (int) -1;

#endif // TYPES_H
