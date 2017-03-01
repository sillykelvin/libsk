#ifndef METADATA_ALLOCATOR_H
#define METADATA_ALLOCATOR_H

#include "shm/shm_mgr.h"
#include "log/log.h"
#include "utility/types.h"
#include "utility/config.h"
#include "utility/assert_helper.h"
#include "shm/detail/offset_ptr.h"

namespace sk {
namespace detail {

/**
 * An allocator to allocate space for metadata objects.
 */
template<typename T>
struct metadata_allocator {
    static_assert(sizeof(T) <= META_ALLOC_INCREMENT, "T must NOT be so big");

    offset_ptr<T> free_list;
    offset_t      free_space;
    size_t        space_left;

    struct {
        size_t total_size;  // total size allocated from shared memory
        size_t waste_size;  // wasted memory size
        size_t alloc_count; // how many allocation of T has happened
        size_t free_count;  // how many deallocation of T has happened
    } stat;

    void init() {
        free_list.set_null();
        free_space = OFFSET_NULL;
        space_left = 0;

        memset(&stat, 0x00, sizeof(stat));
    }

    void report() {
        sk_info("metadata allocator => sizeof(T): %lu.", sizeof(T));
        sk_info("metadata allocator => allocated size: %lu, wasted size: %lu.",
                stat.total_size, stat.waste_size);
        sk_info("metadata allocator => allocation count: %lu, deallocation count: %lu.",
                stat.alloc_count, stat.free_count);
    }

    offset_ptr<T> allocate() {
        // 1. if there are objects in free list
        if (free_list) {
            offset_ptr<T> ret = free_list;
            free_list = *cast_ptr(offset_ptr<T>, free_list.get());
            stat.alloc_count += 1;
            return ret;
        }

        // 2. if no enough space in free space
        if (space_left < sizeof(T)) {
            offset_ptr<void> ptr(shm_mgr::get()->allocate_metadata(META_ALLOC_INCREMENT));
            if (!ptr)
                return offset_ptr<T>::null();

            stat.waste_size += space_left;
            space_left = META_ALLOC_INCREMENT;
            free_space = ptr.offset;
            stat.total_size += space_left;
        }

        offset_ptr<T> ptr(free_space);

        space_left -= sizeof(T);
        free_space += sizeof(T);
        stat.alloc_count += 1;

        return ptr;
    }

    void deallocate(offset_ptr<T> ptr) {
        if (!ptr) {
            sk_assert(0);
            return;
        }

        *cast_ptr(offset_ptr<T>, ptr.get()) = free_list;
        free_list = ptr;
        stat.free_count += 1;
    }
};

} // namespace detail
} // namespace sk

#endif // METADATA_ALLOCATOR_H
