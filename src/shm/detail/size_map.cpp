#include "libsk.h"
#include "size_map.h"

namespace sk {
namespace detail {

static int lg_floor(size_t n) {
    int log = 0;
    for (int i = 4; i >= 0; --i) {
        int shift = 1 << i;
        size_t x = n >> shift;
        if (x != 0) {
            n = x;
            log += shift;
        }
    }

    assert_noeffect(n == 1);
    return log;
}

static int calc_alignment(size_t bytes) {
    const int min_align = 16;
    int alignment = ALIGNMENT;

    if (bytes > MAX_SIZE)
        alignment = PAGE_SIZE;
    else if (bytes >= 128)
        alignment = (1 << lg_floor(bytes)) / 8;
    else if (bytes >= (size_t) min_align)
        alignment = min_align;

    if (alignment > PAGE_SIZE)
        alignment = PAGE_SIZE;

    assert_noeffect(bytes < (size_t)min_align || alignment >= min_align);
    assert_noeffect((alignment & (alignment - 1)) == 0);
    return alignment;
}

static int chunk_count(size_t bytes) {
    if (bytes == 0) return 0;

    int num = static_cast<int>(64.0 * 1024.0 / bytes);
    if (num < 2) num = 2;
    if (num > 512) num = 512;

    return num;
}

int size_map::init() {
    if (__index(0) != 0) {
        ERR("invalid class index for 0: %u.", __index(0));
        return -1;
    }

    if (__index(MAX_SIZE) >= array_len(index2class)) {
        ERR("invalid class index for MAX_SIZE: %u.", __index(MAX_SIZE));
        return -1;
    }

    memset(this, 0x00, sizeof(*this));

    int sc = 0;
    int alignment = ALIGNMENT;
    for (size_t size = ALIGNMENT; size <= MAX_SIZE; size += alignment) {
        alignment = calc_alignment(size);
        assert_retval(size % alignment == 0, -1);

        int chunk_to_alloc = chunk_count(size) / 4;
        size_t psize = 0;
        do {
            psize += PAGE_SIZE;
            // make sure the wasted space is at most 1/8 of total space
            while ((psize % size) > (psize >> 3)) psize += PAGE_SIZE;
        } while ((psize / size) < (size_t) chunk_to_alloc);

        const int this_page_count = psize >> PAGE_SHIFT;
        if (sc > 0 && this_page_count == class2pages[sc - 1]) {
            const int this_chunk_count = (this_page_count << PAGE_SHIFT) / size;
            const int prev_chunk_count = (class2pages[sc - 1] << PAGE_SHIFT) / class2size[sc - 1];

            // chunk count of this size class is same as the previous one, so we merge
            // the two size class into one to reduce fragmentation
            if (this_chunk_count == prev_chunk_count) {
                class2size[sc - 1] = size;
                continue;
            }
        }

        class2pages[sc] = this_page_count;
        class2size[sc] = size;
        ++sc;
    }

    if (sc != SIZE_CLASS_COUNT) {
        ERR("wrong number of size classes, found: %d, expected: %d.", sc, SIZE_CLASS_COUNT);
        return -1;
    }

    // fill the size -> index -> size class map
    int next_size = 0;
    for (int c = 0; c < SIZE_CLASS_COUNT; ++c) {
        const int size = class2size[c];
        for (int s = next_size; s <= size; s+= ALIGNMENT)
            index2class[__index(s)] = c;

        next_size = size + ALIGNMENT;
    }

    for (size_t size = 0; size <= MAX_SIZE;) {
        const int sc = size_class(size);
        if (sc < 0 || sc >= SIZE_CLASS_COUNT) {
            ERR("bad size class, class: %d, size: %lu.", sc, size);
            return -1;
        }

        if (sc > 0 && size <= class2size[sc - 1]) {
            ERR("size class too large, class: %d, size: %lu.", sc, size);
            return -1;
        }

        const size_t s = class2size[sc];
        if (size > s || s == 0) {
            ERR("bad class: %d, size: %lu.", sc, size);
            return -1;
        }

        if (size <= MAX_SMALL_SIZE)
            size += 8;
        else
            size += 128;
    }

    return 0;
}

} // namespace detail
} // namespace sk
