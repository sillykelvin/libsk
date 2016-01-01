#ifndef CONFIG_H
#define CONFIG_H

static const int MAGIC      = 0xC0DEFEED;     // "code feed" :-)
static const int ALIGN_SIZE = 8;              // memory align size, 8 bytes
static const int ALIGN_MASK = ALIGN_SIZE - 1;

/*
 * page size, 8KB
 */
static const int PAGE_SHIFT = 13;
static const int PAGE_SIZE  = 1 << PAGE_SHIFT;

/*
 * the maximum memory size can be managed, 32GB
 */
static const int MAX_MEM_SHIFT   = 35;
static const size_t MAX_MEM_SIZE = 1ULL << MAX_MEM_SHIFT;

/*
 * mata data alignment, 8 byte
 */
static const int META_ALIGN_SHIFT = 3;
static const int META_ALIGN_SIZE  = 1 << META_ALIGN_SHIFT;

/*
 * allocation size for metadata once, 8KB
 * note: make sure this size is aligned by META_ALIGN_SIZE
 */
static const size_t META_ALLOC_SIZE = 8 * 1024;

#endif // CONFIG_H
