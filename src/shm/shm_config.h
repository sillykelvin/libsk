#ifndef SHM_CONFIG_H
#define SHM_CONFIG_H

#include <limits.h>
#include <utility/types.h>

NS_BEGIN(sk)

using shm_page_t   = size_t;
using shm_offset_t = size_t;
using shm_serial_t = size_t;

struct shm_config {
    /*
     * on 64 bit Linux, only 48 bits are used for addresses
     */
    static const size_t ADDRESS_BITS = 48;
    static const size_t ADDRESS_MASK = (1ULL << ADDRESS_BITS) - 1;

    /*
     * page size, 8KB
     */
    static const size_t PAGE_BITS = 13;
    static const size_t PAGE_SIZE = 1ULL << PAGE_BITS;
    static const size_t PAGE_MASK = PAGE_SIZE - 1;

    static const size_t SERIAL_BITS = sizeof(u64) * CHAR_BIT - ADDRESS_BITS;
    static const size_t SERIAL_MASK = (1ULL << SERIAL_BITS) - 1;

    static const size_t METADATA_SERIAL_NUM  = 1;
    static const size_t USERDATA_SERIAL_NUM  = 2;
    static const size_t MIN_VALID_SERIAL_NUM = 3;

    /*
     * TODO: refine this comment and rename this field
     * for spans with page count < MAX_PAGES will be stored in
     * a page-count-indexed span array to speed up query, 128
     */
    static const size_t MAX_PAGES = 1ULL << (20 - PAGE_BITS);

    /*
     * the minimum and maximum memory we can fetch from system
     * every time when there is no enough memory in page heap,
     * the minimum size is 1MB (128 pages)
     *
     * REQUIRED: HEAP_GROW_PAGE_COUNT <= MAX_PAGES
     * TODO: find why this requirement is needed
     */
    static const size_t HEAP_GROW_PAGE_COUNT = MAX_PAGES;

    static const size_t MAX_PAGE_COUNT = 1ULL << (ADDRESS_BITS - PAGE_BITS);

    static const size_t METADATA_GROW_SIZE = 1ULL << 20;
    static const size_t USERDATA_GROW_SIZE = 1ULL << 20;

    /*
     * the maximum path byte count, including the trailing null character
     */
    static const size_t MAX_PATH_SIZE = 256;
};

NS_END(sk)

#endif // SHM_CONFIG_H
