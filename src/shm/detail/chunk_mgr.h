#ifndef CHUNK_MGR_H
#define CHUNK_MGR_H

namespace sk {
namespace detail {

struct lru {
    int    head;
    int    tail;
    size_t chunk_size;
    char   *pool;

    mem_chunk *__at(int index);

    void __unlink(int index);

    void __push_back(int index);

    int init(char *pool, size_t chunk_size, bool resume);

    bool empty();

    int pop();

    int renew(int index);

    void remove(int index);
};

struct chunk_mgr {
    size_t chunk_size;
    size_t bucket_size;
    size_t used_chunk_count;
    size_t total_chunk_count;
    lru    lru_cache;
    int    *partial_buckets;  // hash buckets, point to chunks which are partially allocated
    int    *empty_buckets;    // hash buckets, point to chunks which are totally empty
    char   *pool;             // the base address of entire chunk pool
    char   data[0];

    static size_t calc_size(size_t bucket_size) {
        return sizeof(chunk_mgr) + bucket_size * sizeof(int) * 2;
    }

    static chunk_mgr *create(void *addr, size_t size, bool resume, size_t bucket_size,
                             size_t chunk_size, size_t total_chunk_count);

    int init(char *pool);

    mem_chunk *__at(int index, bool check_magic = true);

    void *index2ptr(int chunk_index, int block_index);

    int malloc(size_t mem_size, int& chunk_index, int& block_index);

    void free(int chunk_index, int block_index);
};

} // namespace detail
} // namespace sk

#endif // CHUNK_MGR_H
