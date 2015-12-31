#ifndef MEM_CHUNK_H
#define MEM_CHUNK_H

namespace sk {
namespace detail {

/**
 * @brief The mem_chunk struct represents a memory chunk, which
 * consists many small blocks with same size, these blocks are
 * linked as a single linked list.
 *
 * NOTE: a chunk may contain many blocks, at lease 1 block
 *
 *    +-----------+   ---
 *    | mem_chunk |    |      block_size
 *    +-----------+    |         ---
 *    |  block 0  |    |          |
 *    +-----------+ chunk_size   ---
 *    |    ...    |    |
 *    +-----------+    |
 *    |  block N  |    |
 *    +-----------+   ---
 *
 * This struct will always be fetched from a raw memory address,
 * so we do not provide a constructor here, we provide a init()
 * method instead.
 */
struct mem_chunk {
    int magic;          // for memory overflow verification
    int prev_lru;       // prev chunk index in lru list
    int next_lru;       // next chunk index in lru list
    int next_chunk;     // next chunk index in allocation list
    int free_head;      // free head index
    int free_count;     // free block count
    int total_count;    // total block count
    size_t block_size;  // block size
    char data[0];

    bool full() const;

    bool empty() const;

    int init(size_t chunk_size, size_t block_size);

    /**
     * @brief malloc will allocate a block from this chunk
     * @return block index if succeeded, negative value if not
     */
    int malloc();

    /**
     * @brief free will deallocate a block in this chunk
     * @param block_index is the block index in the chunk
     */
    void free(int block_index);
};

} // namespace detail
} // namespace sk

#endif // MEM_CHUNK_H
