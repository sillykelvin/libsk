#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

typedef uint64_t u64;
typedef uint32_t u32;

typedef u64 shm_ptr;

static const u64 SHM_NULL = (u64) -1;
static const int IDX_NULL = (int) -1;

#endif // TYPES_H
