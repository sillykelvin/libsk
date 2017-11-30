#include <shm/shm_config.h>
#include <shm/detail/size_map.h>

using namespace sk::detail;

static int chunk_count(size_t bytes) {
    if (bytes == 0) return 0;

    int num = cast_int(64.0 * 1024.0 / bytes);
    if (num < 2)  num = 2;
    if (num > 32) num = 32;

    return num;
}

size_t size_map::lg_floor(size_t n) {
    size_t log = 0;
    for (int i = 4; i >= 0; --i) {
        int shift = 1 << i;
        size_t x = n >> shift;
        if (x != 0) {
            n = x;
            log += shift;
        }
    }

    sk_assert(n == 1);
    return log;
}

size_t size_map::calc_alignment(size_t bytes) {
    size_t alignment = BASE_ALIGNMENT;

    do {
        if (bytes > MAX_SIZE) {
            alignment = shm_config::PAGE_SIZE;
            break;
        }

        if (bytes >= 128) {
            alignment = (1ULL << lg_floor(bytes)) / 8;
            break;
        }

        if (bytes >= MIN_ALIGNMENT) {
            alignment = MIN_ALIGNMENT;
            break;
        }
    } while (0);

    if (alignment > shm_config::PAGE_SIZE)
        alignment = shm_config::PAGE_SIZE;

    sk_assert(bytes < MIN_ALIGNMENT || alignment >= MIN_ALIGNMENT);
    sk_assert((alignment & (alignment - 1)) == 0);
    return alignment;
}

int size_map::init() {
    if (class_index(0) != 0) {
        sk_error("invalid class index for 0: %lu", class_index(0));
        return -1;
    }

    if (class_index(MAX_SIZE) >= sizeof(index2class_)) {
        sk_error("invalid class index for MAX_SIZE: %lu", class_index(MAX_SIZE));
        return -1;
    }

    memset(this, 0x00, sizeof(*this));

    u8 sc = 1;
    size_t alignment = BASE_ALIGNMENT;
    for (size_t size = BASE_ALIGNMENT; size <= MAX_SIZE; size += alignment) {
        alignment = calc_alignment(size);
        sk_assert(size % alignment == 0);

        int chunk_to_alloc = chunk_count(size) / 4;
        size_t psize = 0;
        do {
            psize += shm_config::PAGE_SIZE;
            // allocate enough pages so leftover is less than 1/8
            // of total, this bounds wasted space to at most 12.5%
            while ((psize % size) > (psize >> 3))
                psize += shm_config::PAGE_SIZE;
        } while ((psize / size) < cast_size(chunk_to_alloc));

        const size_t this_page_count = psize >> shm_config::PAGE_SHIFT;
        if (sc > 1 && this_page_count == class2pages_[sc - 1]) {
            // see if we can merge this into the previous class without
            // increasing the fragmentation of the previous class
            const size_t this_chunk_count = (this_page_count << shm_config::PAGE_SHIFT) / size;
            const size_t prev_chunk_count = (class2pages_[sc - 1] << shm_config::PAGE_SHIFT) / class2size_[sc - 1];

            if (this_chunk_count == prev_chunk_count) {
                // adjust last class to include this size
                class2size_[sc - 1] = size;
                continue;
            }
        }

        // add new class
        class2pages_[sc] = this_page_count;
        class2size_[sc] = size;
        ++sc;
    }

    if (sc > SIZE_CLASS_COUNT) {
        sk_error("too many size classes, found: %u, max: %u", sc, SIZE_CLASS_COUNT);
        return -1;
    }

    size_t next_size = 0;
    const u8 class_count = sc;
    for (u8 c = 1; c < class_count; ++c) {
        const size_t max_size_in_class = class2size_[c];
        for (size_t s = next_size; s <= max_size_in_class; s += BASE_ALIGNMENT)
            index2class_[class_index(s)] = c;

        next_size = max_size_in_class + BASE_ALIGNMENT;
    }

    for (size_t size = 0; size <= MAX_SIZE;) {
        u8 sc = 0;
        bool ok = size2class(size, &sc);
        if (!ok || sc <= 0 || sc >= class_count) {
            sk_error("bad size class, class: %u, size: %lu", sc, size);
            return -1;
        }

        if (sc > 1 && size <= class2size_[sc - 1]) {
            sk_error("size class too large, class: %u, size: %lu", sc, size);
            return -1;
        }

        const size_t s = class2size_[sc];
        if (size > s || s == 0) {
            sk_error("bad class: %u, size: %lu", sc, size);
            return -1;
        }

        if (size <= MAX_SMALL_SIZE)
            size += 8;
        else
            size += 128;
    }

    return 0;
}
