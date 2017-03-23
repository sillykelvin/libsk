#ifndef CONFIG_H
#define CONFIG_H

#include "utility/types.h"

/*
 * sometimes, we need a magic code to identify something
 * "code feed" :-)
 */
static const int MAGIC = 0xC0DEFEED;

/*
 * page size, 8KB
 */
static const int PAGE_SHIFT = 13;
static const int PAGE_SIZE  = 1 << PAGE_SHIFT;

/*
 * the maximum size that will be treated as "small objects", size
 * smaller than this will have an alignment according to actual
 * size, and also the allocation size will be fixed up to a
 * "reasonable" size; size bigger than this will be aligned by
 * page size, and the allocation size will be the minimum pages
 * which can hold the desired size
 */
static const size_t MAX_SIZE = 256 * 1024;

/*
 * the count of different pre-defined allocation size for "small
 * objects", one allocation size is a "size class", this value is
 * bound to MAX_SIZE, if MAX_SIZE changes, this value may change too
 */
static const int SIZE_CLASS_COUNT = 87;

/*
 * the maximum memory size can be managed, 32GB
 */
static const int MAX_MEM_SHIFT   = 35;
static const size_t MAX_MEM_SIZE = 1ULL << MAX_MEM_SHIFT;

/*
 * for spans with pages < MAX_PAGES will be stored in
 * a page-count-indexed span array to speed up query, 128
 */
static const int MAX_PAGES = 1 << (20 - PAGE_SHIFT);

/*
 * allocation size when there is no enough space for metadata objects,
 * it is used in metadata_allocator, 128KB
 */
static const size_t META_ALLOC_INCREMENT = 128 * 1024;

/*
 * mininum number of pages to fetch from shared memory segment
 * at a time when there is no enough memory in page heap
 */
static const int HEAP_GROW_PAGE_COUNT = MAX_PAGES;

/*
 * max pages number can be managed, 19bits
 */
static const int MAX_VALID_PAGES = static_cast<int>(~static_cast<u32>(0) >> PAGE_SHIFT);

/*
 * basic alignment, 8
 */
static const int ALIGNMENT = 8;

/*
 * mininum metadata space should reserve, 2MB
 */
static const size_t MIN_METADATA_SPACE = 2 * 1024 * 1024;

/*
 * minimum space should a shm_mgr manage, 3MB
 */
static const size_t MIN_SHM_SPACE = 3 * 1024 * 1024;

#endif // CONFIG_H
