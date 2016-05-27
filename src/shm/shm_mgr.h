#ifndef SHM_MGR_H
#define SHM_MGR_H

namespace sk {

int shm_mgr_init(key_t key, size_t size_hint, bool resume);
int shm_mgr_fini();

namespace detail {

struct size_map;
struct chunk_cache;
struct page_heap;

}

template<typename T>
struct shm_ptr;

struct shm_mgr {
    /*
     * id of the shm pool
     */
    int shmid;

    /*
     * total memory size and used memory size
     */
    size_t total_size;
    size_t used_size;

    /*
     * metadata allocation
     */
    offset_t metadata_offset;
    size_t metadata_left;

    /*
     * runtime statistics
     */
    struct {
        size_t alloc_count;      // total allocation count
        size_t free_count;       // total deallocation count
        size_t raw_alloc_count;  // raw memory allocation count
        size_t meta_alloc_count; // metadata allocation count
    } stat;

    /*
     * pointer to size map, page heap and chunk cache
     */
    detail::size_map *size_map;
    detail::chunk_cache *chunk_cache;
    detail::page_heap *page_heap;

    static shm_mgr *create(key_t key, size_t size_hint, bool resume);

    static shm_mgr *get();

    int init(bool resume);

    void report();

    shm_ptr<void> malloc(size_t bytes);
    void free(shm_ptr<void> ptr);

    offset_t __sbrk(size_t bytes);

    /*
     * allocate raw memory for high level usage,
     * the size should be N * PAGE_SIZE (N > 1),
     * otherwise, it will be fixed up to a proper
     * page count * PAGE_SIZE
     */
    offset_t allocate(size_t bytes);

    /*
     * allocate raw memory for metadata objects
     */
    offset_t allocate_metadata(size_t bytes);

    /*
     * turn a valid offset into a raw pointer
     */
    void *offset2ptr(offset_t offset);

    /*
     * turn a raw pointer into an offset, returns
     * OFFSET_NULL if ptr is out of range
     */
    offset_t ptr2offset(void *ptr);

    /*
     * get the page number of the pointer
     */
    page_t ptr2page(shm_ptr<void> ptr);
};



/*
 * the group of functions below is used to manipulate shared memory allocation/deallocation.
 *
 * shm_malloc/shm_free is like malloc/free in C, only memories are allocated/deallocated,
 * no constructors/destructors are called
 *
 * shm_new/shm_del are like new/delete in C++, they will call constructors/destructors
 * when allocating/deallocating memories
 */
shm_ptr<void> shm_malloc(size_t bytes);
void shm_free(shm_ptr<void> ptr);


template<typename T, typename... Args>
shm_ptr<T> shm_new(Args&&... args) {
    shm_ptr<T> ptr(shm_malloc(sizeof(T)));
    if (!ptr)
        return ptr;

    T *t = ptr.get();
    new (t) T(std::forward<Args>(args)...);

    return ptr;
}

template<typename T>
void shm_del(shm_ptr<T> ptr) {
    if (!ptr)
        return;

    T *t = ptr.get();
    t->~T();

    shm_free(ptr);
}

} // namespace sk

#endif // SHM_MGR_H
