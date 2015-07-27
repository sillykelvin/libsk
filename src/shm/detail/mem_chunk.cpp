#include "libsk.h"
#include "mem_chunk.h"

int sk::detail::mem_chunk::init(size_t chunk_size, size_t block_size) {
    assert_retval(chunk_size >= sizeof(mem_chunk) + block_size, -EINVAL);
    assert_retval(block_size % ALIGN_SIZE == 0, -EINVAL);
    assert_retval(block_size >= sizeof(int), -EINVAL);

    magic = MAGIC;
    free_head = 0;
    free_count = (chunk_size - sizeof(mem_chunk)) / block_size;
    total_count = free_count;
    this->block_size = block_size;

    // link the free blocks
    int *n = NULL;
    for (int i = 0; i < free_count; ++i) {
        n = static_cast<int *>(static_cast<void *>(data + i * block_size));
        *n = i + 1;
    }
    *n = IDX_NULL; // the last block

    return 0;
}
