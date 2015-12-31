#ifndef CONFIG_H
#define CONFIG_H

static const int MAGIC      = 0xC0DEFEED;     // "code feed" :-)
static const int ALIGN_SIZE = 8;              // memory align size, 8 bytes
static const int ALIGN_MASK = ALIGN_SIZE - 1;

/*
 * page size, 8KB
 */
static const int PAGE_SHIFT = 13;
static const int PAGE_SIZE = 1 << PAGE_SHIFT;

/*
 * the maximum memory size can be managed, 32GB
 */
static const int MAX_MEM_SHIFT = 35;
static const size_t MAX_MEM_SIZE = 1ULL << MAX_MEM_SHIFT;

#endif // CONFIG_H
