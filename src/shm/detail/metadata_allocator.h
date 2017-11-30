#ifndef METADATA_ALLOCATOR_H
#define METADATA_ALLOCATOR_H

#include <shm/shm_config.h>
#include <utility/math_helper.h>
#include <shm/detail/shm_address.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

template<typename T>
class metadata_allocator {
    static_assert(sizeof(T) >= sizeof(shm_address), "sizeof(T) underflow");
    static_assert(sizeof(T) <= shm_config::METADATA_ALLOCATION_SIZE, "sizeof(T) overflow");
public:
    shm_address allocate() {
        do {
            if (free_list_) break;

            shm_address space = shm_mgr::allocate_metadata(shm_config::METADATA_ALLOCATION_SIZE);
            if (!space) return nullptr;

            shm_address block(space.block());

            char *base = char_ptr(block.get());
            char *ptr  = char_ptr(space.get());
            char *end  = sk::byte_offset<char>(ptr, shm_config::METADATA_ALLOCATION_SIZE);
            while (ptr + sizeof(T) <= end) {
                *cast_ptr(shm_address, ptr) = free_list_;
                free_list_ = shm_address(space.block(), ptr - base);
                ptr += sizeof(T);
            }

            sk_assert(ptr <= end);
            stat_.waste_size += (end - ptr);
            stat_.total_size += shm_config::METADATA_ALLOCATION_SIZE;
        } while (0);

        assert_retval(free_list_, nullptr);

        shm_address addr = free_list_;
        free_list_ = *free_list_.as<shm_address>();
        stat_.alloc_count += 1;
        return addr;
    }

    void deallocate(shm_address addr) {
        assert_retnone(addr);

        *addr.as<shm_address>() = free_list_;
        free_list_ = addr;
        stat_.free_count += 1;
    }

private:
    shm_address free_list_;

    struct stat {
        stat() { memset(this, 0x00, sizeof(*this)); }

        size_t total_size;  // total memory size fetched from system
        size_t waste_size;  // wasted memory size
        size_t alloc_count; // allocation count
        size_t free_count;  // deallocation count
    } stat_;
};

NS_END(detail)
NS_END(sk)

#endif // METADATA_ALLOCATOR_H
