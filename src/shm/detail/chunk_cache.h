#ifndef CHUNK_CACHE_H
#define CHUNK_CACHE_H

#include "shm/detail/span.h"
#include "utility/config.h"

namespace sk {
namespace detail {

struct chunk_cache {
    span free_lists[SIZE_CLASS_COUNT];

    // TODO: add stat field here

    void init();

    shm_ptr<void> allocate(size_t bytes);
    void deallocate(shm_ptr<void> ptr);
};

} // namespace detail
} // namespace sk

#endif // CHUNK_CACHE_H
