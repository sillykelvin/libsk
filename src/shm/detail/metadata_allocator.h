#ifndef METADATA_ALLOCATOR_H
#define METADATA_ALLOCATOR_H

namespace sk {
namespace detail {

/**
 * An allocator to allocate space for metadata objects.
 */
template<typename T>
struct metadata_allocator {
    offset_t free_list;
    offset_t free_space;
    size_t   space_left;

    struct {
        size_t total_size;  // total size allocated from shared memory
        size_t waste_size;  // wasted memory size
        size_t alloc_count; // how many allocation of T has happened
        size_t free_count;  // how many deallocation of T has happened
    } stat;

    void init() {
        assert_noeffect(sizeof(T) <= META_ALLOC_INCREMENT);
        free_list = OFFSET_NULL;
        free_space = OFFSET_NULL;
        space_left = 0;

        memset(&stat, 0x00, sizeof(stat));
    }

    T *allocate(offset_t *offset) {
        // 1. if there are objects in free list
        if (free_list != OFFSET_NULL) {
            // TODO: may replace with shm_ptr here, same as below
            void *addr = shm_mgr::get()->offset2ptr(free_list);
            if (offset)
                *offset = free_list;

            free_list = *cast_ptr(offset_t, addr);
            stat.alloc_count += 1;

            return cast_ptr(T, addr);
        }

        // 2. if no enough space in free space
        if (space_left < sizeof(T)) {
            stat.waste_size += space_left;

            space_left = META_ALLOC_INCREMENT;
            void *addr = shm_mgr::get()->allocate_metadata(space_left, &free_space);
            if (!addr)
                return NULL;

            stat.total_size += space_left;
        }

        void *addr = shm_mgr::get()->offset2ptr(free_space);
        if (offset)
            *offset = free_space;

        space_left -= sizeof(T);
        free_space += sizeof(T);
        stat.alloc_count += 1;

        return cast_ptr(T, addr);
    }

    void deallocate(offset_t offset) {
        void *addr = shm_mgr::get()->offset2ptr(offset);
        *cast_ptr(offset_t, addr) = free_list;
        free_list = offset;
        stat.free_count += 1;
    }
};

} // namespace detail
} // namespace sk

#endif // METADATA_ALLOCATOR_H
