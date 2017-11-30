#ifndef SHM_MGR_H
#define SHM_MGR_H

#include <string.h>
#include <shm/shm_ptr.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

class size_map;
class page_heap;
class chunk_cache;

class shm_mgr {
public:
    shm_mgr();
    ~shm_mgr();

    int on_create(const char *basename);
    int on_resume(const char *basename);

    shm_ptr<void> malloc(size_t bytes);
    void free(shm_ptr<void> ptr);

    bool has_singleton(int id);
    shm_ptr<void> get_singleton(int id, size_t bytes, bool *first_call);
    void free_singleton(int id);

    shm_address allocate_metadata(size_t *bytes);
    shm_address allocate_userdata(size_t *bytes);

    void *addr2ptr(shm_address addr);
    shm_address ptr2addr(const void *ptr);

    class size_map *size_map() { return size_map_; }
    class page_heap *page_heap() { return page_heap_; }

private:
    struct shm_block {
        shm_block() : addr(nullptr), used_size(0), real_size(0), mmap_size(0) {}

        void *addr;
        size_t used_size;
        size_t real_size;
        size_t mmap_size;
    };

    void calc_path(int block_index, char *path, size_t size) {
        const char *suffix_name[] = { "metadata", "userdata" };
        snprintf(path, size, "%s-%s.mmap", basename_, suffix_name[block_index]);
    }

    shm_address sbrk(int block_index, size_t *bytes);

    int create_block(int block_index, size_t real_size, size_t mmap_size);
    int resize_block(int block_index, size_t new_real_size);
    int attach_block(int block_index);
    int unlink_block(int block_index);

private:
    static const int METADATA_BLOCK = 0;
    static const int USERDATA_BLOCK = 1;
    static const int MAX_SINGLETON_COUNT = 256;

    shm_serial_t serial_;
    size_t default_mmap_size_;
    char basename_[shm_config::MAX_PATH_SIZE];
    shm_ptr<void> singletons_[MAX_SINGLETON_COUNT];

    // blocks_[METADATA_BLOCK] for metadata allocation
    // blocks_[USERDATA_BLOCK] for userdata allocation
    shm_block blocks_[2];

    struct stat {
        stat() { memset(this, 0x00, sizeof(*this)); }

        size_t alloc_count;          // total allocation count
        size_t free_count;           // total deallocation count
        size_t metadata_alloc_count; // metadata allocation count
        size_t userdata_alloc_count; // userdata allocation count
    } stat_;

    class size_map    *size_map_;
    class page_heap   *page_heap_;
    class chunk_cache *chunk_cache_;
};

NS_END(detail)
NS_END(sk)

#endif // SHM_MGR_H
