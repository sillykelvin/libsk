#ifndef MEM_CHUNK_H
#define MEM_CHUNK_H

namespace sk {

static const int MAGIC      = 0xC0DEFEED;     // "code feed" :-)
static const int ALIGN_SIZE = 8;              // memory align size, 8 bytes
static const int ALIGN_MASK = ALIGN_SIZE - 1;

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
    int free_head;      // free head index
    int free_count;     // free block count
    int total_count;    // total block count
    size_t block_size;  // block size
    char data[0];

    int init(size_t chunk_size, size_t block_size);

    bool full() const {
        assert_retval(magic == MAGIC, true); // mark the block as full if overflowed
        return free_count <= 0;
    }

    bool empty() const {
        assert_retval(magic == MAGIC, false);
        return free_count >= total_count;
    }

    /**
     * @brief malloc will allocate a block from this chunk
     * @return block index if succeeded, negative value if not
     */
    int malloc() {
        if (full())
            return -ENOMEM;

        int block_index = free_head;
        free_head = *(static_cast<int *>(static_cast<void *>(data + free_head * block_size)));
        --free_count;

        return block_index;
    }

    /**
     * @brief free will deallocate a block in this chunk
     * @param block_index is the block index in the chunk
     */
    void free(int block_index) {
        assert_retnone(magic == MAGIC);
        assert_retnone(!empty());
        assert_retnone(block_index >= 0 && block_index < total_count);

        *(static_cast<int *>(static_cast<void *>(data + block_size * block_index))) = free_head;
        free_head = block_index;
        ++free_count;
    }
};

} // namespace detail
} // namespace sk

#endif // MEM_CHUNK_H
