#ifndef METADATA_ALLOCATOR_H
#define METADATA_ALLOCATOR_H

#include "shm/shm_mgr.h"
#include "shm/shm_ptr.h"
#include "utility/log.h"
#include "utility/types.h"
#include "utility/config.h"
#include "utility/assert_helper.h"

namespace sk {
namespace detail {

/**
 * An allocator to allocate space for metadata objects.
 */
template<typename T>
struct metadata_allocator {
    shm_ptr<T> free_list;
    offset_t   free_space;
    size_t     space_left;

    struct {
        size_t total_size;  // total size allocated from shared memory
        size_t waste_size;  // wasted memory size
        size_t alloc_count; // how many allocation of T has happened
        size_t free_count;  // how many deallocation of T has happened
    } stat;

    void init() {
        assert_noeffect(sizeof(T) <= META_ALLOC_INCREMENT);
        free_list = SHM_NULL;
        free_space = OFFSET_NULL;
        space_left = 0;

        memset(&stat, 0x00, sizeof(stat));
    }

    void report() {
        INF("metadata allocator => sizeof(T): %lu.", sizeof(T));
        INF("metadata allocator => allocated size: %lu, wasted size: %lu.",
            stat.total_size, stat.waste_size);
        INF("metadata allocator => allocation count: %lu, deallocation count: %lu.",
            stat.alloc_count, stat.free_count);
    }

    shm_ptr<T> allocate() {
        // 1. if there are objects in free list
        if (free_list) {
            shm_ptr<T> ret = free_list;
            free_list = *cast_ptr(shm_ptr<T>, free_list.get());
            stat.alloc_count += 1;
            return ret;
        }

        // 2. if no enough space in free space
        if (space_left < sizeof(T)) {
            shm_ptr<void> ptr(shm_mgr::get()->allocate_metadata(META_ALLOC_INCREMENT));
            if (!ptr)
                return SHM_NULL;

            stat.waste_size += space_left;
            space_left = META_ALLOC_INCREMENT;
            free_space = ptr.offset;
            stat.total_size += space_left;
        }

        shm_ptr<T> ptr(free_space);

        space_left -= sizeof(T);
        free_space += sizeof(T);
        stat.alloc_count += 1;

        return ptr;
    }

    void deallocate(shm_ptr<T> ptr) {
        if (!ptr) {
            assert_noeffect(0);
            return;
        }

        *cast_ptr(shm_ptr<T>, ptr.get()) = free_list;
        free_list = ptr;
        stat.free_count += 1;
    }
};

} // namespace detail
} // namespace sk

#endif // METADATA_ALLOCATOR_H
