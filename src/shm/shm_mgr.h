#ifndef SHM_MGR_H
#define SHM_MGR_H

namespace sk {

enum singleton_type {
    ST_MIN = 0,

    // ...

    ST_MAX
};

int shm_register_singleton(int id, size_t size);

int shm_mgr_init(key_t key, bool resume, size_t max_block_size, int chunk_count, size_t heap_size);
int shm_mgr_fini();



namespace detail {

struct chunk_mgr;
struct mem_chunk;
struct buddy;

} // namespace detail

namespace detail {

// we should not use 0 here, we will leave
// 0 to the invalid pointer type
enum detail_ptr_type {
    PTR_TYPE_SINGLETON = 1,
    PTR_TYPE_CHUNK     = 2,
    PTR_TYPE_HEAP      = 3
};

/*
 * 1. if the ptr points to a singleton object:
 *    idx1: singleton id
 *    idx2: unused
 * 2. if the ptr points to a chunk block:
 *    idx1: chunk index
 *    idx2: block index
 * 3. if the ptr points to heap:
 *    idx1: unit index
 *    idx2: unused
 */
struct detail_ptr {
    int idx1: 32;
    int idx2: 30;
    u32 type: 2;
};

} // namespace detail


template<typename T>
struct shm_ptr;


/**
 * the layout of shm_mgr:
 *
 *     +---------+-----------+-----------+-----------+-------+------+
 *     |         | singleton |           |   heap    | chunk | heap |
 *     | shm_mgr |           | chunk mgr |           |       |      |
 *     |         |  objects  |           | allocator | pool  | pool |
 *     +---------+-----------+-----------+-----------+-------+------+
 */
struct shm_mgr {
    typedef size_t offset_t;
    typedef detail::chunk_mgr chunk_allocator;
    typedef detail::buddy heap_allocator;

    /*
     * id of the shm pool
     */
    int shmid;

    /*
     * a block is an area inside a chunk, max_block_size is
     * the maximum block can be allocated in chunk pool, so:
     * 1. mem_size <= max_block_size, allocate in chunk pool
     * 2. mem_size > max_block_size, allocate in heap pool
     */
    size_t max_block_size;

    /*
     * the offsets to singleton objects
     */
    offset_t singletons[ST_MAX];

    /*
     * the chunk pool mgr, it manages memory in chunk pool
     */
    chunk_allocator *chunk_mgr;

    /*
     * the heap pool mgr, uses buddy algorithm, it manages
     * memory in heap pool
     */
    heap_allocator *heap_mgr;

    static bool __power_of_two(size_t num);

    static size_t __fix_size(size_t size);

    static size_t __align_size(size_t size);

    static shm_mgr *create(key_t key, bool resume,
                           size_t max_block_size, int chunk_count, size_t heap_size);

    static shm_mgr *get();

    int __malloc_from_chunk_pool(size_t mem_size, int& chunk_index, int& block_index);

    int __malloc_from_heap(size_t mem_size, int& unit_index);

    void __free_from_chunk_pool(int chunk_index, int block_index);

    void __free_from_heap(int unit_index);

    void *mid2ptr(u64 mid);

    void *get_singleton(int id);

    shm_ptr<void> malloc(size_t size);

    void free(shm_ptr<void> ptr);

//---------------------------------------------------------------------------

    /*
     * total memory size and used memory size
     */
    size_t total_size;
    size_t used_size;

    /*
     * statistics of memory allocation
     */
    struct {
    } stat;

    /*
     * allocate raw memory from shared memory, like sbrk() in linux kernel
     * note: it's caller's responsibility to do alignment for size
     */
    void *__sbrk(size_t size, offset_t *offset) {
        // if there is no enough memory, just return
        if (used_size + size > total_size)
            return NULL;

        char *base_addr = char_ptr(this);
        void *ret = base_addr + used_size;
        if (offset)
            *offset = used_size;

        used_size += size;

        return ret;
    }

    void *offset2ptr(offset_t offset) {
        assert_retval(offset >= sizeof(*this), NULL);
        assert_retval(offset < used_size, NULL);

        return char_ptr(this) + offset;
    }
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
shm_ptr<void> shm_malloc(size_t size);
void shm_free(shm_ptr<void> ptr);
void *shm_singleton(int id);


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
