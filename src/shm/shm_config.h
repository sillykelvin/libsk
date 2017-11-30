#ifndef SHM_CONFIG_H
#define SHM_CONFIG_H

#include <utility/types.h>
#include <utility/assert_helper.h>

NS_BEGIN(sk)

class shm_config {
public:
    /*
     * page size, 8KB
     */
    static const size_t PAGE_SHIFT = 13;
    static const size_t PAGE_SIZE  = 1ULL << PAGE_SHIFT;
    static const size_t PAGE_MASK  = PAGE_SIZE - 1;

    /*
     * TODO: refine this comment and rename this field
     * for spans with page count < MAX_PAGES will be stored in
     * a page-count-indexed span array to speed up query, 128
     */
    static const size_t MAX_PAGES = 1ULL << (20 - PAGE_SHIFT);

    /*
     * the minimum and maximum memory we can fetch from system
     * every time when there is no enough memory in page heap,
     * the minimum size is 1MB (128 pages) and the maximum is
     * 4GB (2^19 pages), if the required size is less than the
     * minimum size, than the minimum size is fetched; if the
     * required size is greater than the maximum size, than a
     * null pointer will be returned
     *
     * REQUIRED: HEAP_GROW_PAGE_COUNT <= MAX_PAGES
     * TODO: find why this requirement is needed
     * TODO: the two values should be defined according to other
     * value, MIN_HEAP_GROW_BITS depends on MAX_PAGES, and
     * MAX_HEAP_GROW_BITS depends on MAX_PAGE_BITS
     */
    static const size_t MIN_HEAP_GROW_BITS = 20;
    static const size_t MAX_HEAP_GROW_BITS = 32;
    static const size_t MIN_HEAP_GROW_SIZE = 1ULL << MIN_HEAP_GROW_BITS;
    static const size_t MAX_HEAP_GROW_SIZE = 1ULL << MAX_HEAP_GROW_BITS;
    static const size_t MIN_HEAP_GROW_PAGE_COUNT = 1ULL << (MIN_HEAP_GROW_BITS - PAGE_SHIFT);
    static const size_t MAX_HEAP_GROW_PAGE_COUNT = 1ULL << (MAX_HEAP_GROW_BITS - PAGE_SHIFT);
    static_assert(MAX_PAGES >= MIN_HEAP_GROW_PAGE_COUNT, "invalid MIN_HEAP_GROW_PAGE_COUNT");

    /*
     * allocation alignment when fetching memory from system, 1MB
     * this value should be a reasonable large value, as we need
     * to build a radix tree to map address to block, a large value
     * will reduce the size of that tree
     */
    static const size_t ALIGNMENT_BITS = 20;
    static const size_t ALIGNMENT = 1ULL << ALIGNMENT_BITS;

    /*
     * metadata allocation size when there is no enough space for
     * metadata objects, it's used in metadata_allocator, 128KB
     */
    static const size_t METADATA_ALLOCATION_SIZE = 128 * 1024;

    /*
     * on 64 bit Linux, only 48 bits are used for addresses
     */
    static const size_t ADDRESS_BITS = 48;

    /*
     * the maximum block count, the value should have been 2^28 as the
     * ADDRESS_BITS is 48 and the MIN_HEAP_GROW_BITS is 20, but to add
     * so many information into a u64 integer is impossible, so we have
     * to limit the block count and another field: page count, now the
     * limited block count is 2^16, so the minimum managed memory size
     * is: 2^16 * 2^20 = 2^36 = 64GB, that should be enough
     */
    static const size_t MAX_BLOCK_BITS  = 16;
    static const size_t MAX_BLOCK_COUNT = 1ULL << MAX_BLOCK_BITS;

    /*
     * the maximum page count in one block, same to the above, the value
     * should have been 2^35 as 2^48 / 2^13 = 2^35, but to save the bits
     * in a u64 integer, we limit this value to 2^19, so the maximum memory
     * can be allocated is: 2^19 * 2^13 = 2^32 = 4GB, that should be fairly
     * enough, note that this value is equal to MAX_HEAP_GROW_PAGE_COUNT
     */
    static const size_t MAX_PAGE_BITS  = 19;
    static const size_t MAX_PAGE_COUNT = 1ULL << MAX_PAGE_BITS;
    static_assert(MAX_PAGE_COUNT == MAX_HEAP_GROW_PAGE_COUNT, "invalid MAX_PAGE_COUNT");

    /*
     * the maximum version number, version is increased when process
     * restarts, to indicate that the pointers are invalidated and
     * should be updated, 2^16 = 65536 should be a fairly large number
     * as we do not restart process too often
     */
    static const size_t MAX_VERSION_BITS = 16;
    static const size_t MAX_VERSION_NUM  = 1ULL << MAX_VERSION_BITS;

    /*
     * the maximum offset in a block, it's limited by MAX_HEAP_GROW_BITS
     */
    static const size_t MAX_OFFSET_BITS = MAX_HEAP_GROW_BITS;
    static const size_t MAX_OFFSET_SIZE = 1ULL << MAX_OFFSET_BITS;

    /*
     * the maximum serial number, it's used when allocating objects by user, to
     * identify an unique allocation and to avoid use-after-free error, the
     * SPECIAL_SERIAL is used to identify that it's not an allocation by user
     */
    static const size_t MAX_SERIAL_BITS = 16;
    static const size_t MAX_SERIAL_NUM  = (1ULL << MAX_SERIAL_BITS);
    static const size_t SPECIAL_SERIAL  = 0xC0DE;
    static_assert(SPECIAL_SERIAL < MAX_SERIAL_NUM, "SPECIAL_SERIAL overflow");

    /*
     * the maximum path byte count, including the trailing null character
     */
    static const size_t MAX_PATH_SIZE = 256;
};


NS_END(sk)

#endif // SHM_CONFIG_H
