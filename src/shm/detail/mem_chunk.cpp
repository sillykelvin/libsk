#include "libsk.h"
#include "mem_chunk.h"

int sk::detail::mem_chunk::init(size_t chunk_size, size_t block_size) {
    assert_retval(chunk_size >= sizeof(mem_chunk) + block_size, -EINVAL);
    assert_retval(block_size % ALIGN_SIZE == 0, -EINVAL);
    assert_retval(block_size >= sizeof(int), -EINVAL);

    magic = MAGIC;
    prev_lru = IDX_NULL;
    next_lru = IDX_NULL;
    next_chunk = IDX_NULL;
    free_head = 0;
    free_count = (chunk_size - sizeof(mem_chunk)) / block_size;
    total_count = free_count;
    this->block_size = block_size;

    // link the free blocks
    int *n = NULL;
    for (int i = 0; i < free_count; ++i) {
        n = cast_ptr(int, data + i * block_size);
        *n = i + 1;
    }
    *n = IDX_NULL; // the last block

    return 0;
}

int sk::detail::mem_chunk::malloc() {
    if (full())
        return -ENOMEM;

    int block_index = free_head;
    free_head = *cast_ptr(int, data + free_head * block_size);
    --free_count;

    return block_index;
}

void sk::detail::mem_chunk::free(int block_index) {
    assert_retnone(magic == MAGIC);
    assert_retnone(!empty());
    assert_retnone(block_index >= 0 && block_index < total_count);

    *(cast_ptr(int, data + block_size * block_index)) = free_head;
    free_head = block_index;
    ++free_count;
}
