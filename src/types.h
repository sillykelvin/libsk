#ifndef TYPES_H
#define TYPES_H

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

#define cast_ptr(type, ptr) static_cast<type *>(static_cast<void *>(ptr))
#define void_ptr(ptr)       static_cast<void *>(ptr)
#define char_ptr(ptr)       cast_ptr(char, ptr)

typedef uint64_t u64;
typedef uint32_t u32;

static const size_t INVALID_OFFSET = (size_t) 0;
static const int IDX_NULL = (int) -1;

#endif // TYPES_H
