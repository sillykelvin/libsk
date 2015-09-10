#include <gtest/gtest.h>
#include "libsk.h"
#include "shm/detail/buddy.h"
#include "shm/detail/chunk_mgr.h"
#include "shm/detail/mem_chunk.h"

#define BLK_CNT             (20)
#define BUDDY_SIZE          (60)
#define SHM_MGR_KEY         (0x77777)
#define SHM_MGR_CHUNK_SIZE  (1000)
#define SHM_MGR_CHUNK_COUNT (1024)
#define SHM_MGR_HEAP_SIZE   (10240)

using namespace sk;
using namespace sk::detail;

TEST(shm_mgr, mem_chunk) {
    size_t mem_size = sizeof(mem_chunk) + sizeof(long) * BLK_CNT;
    mem_chunk *chunk = (mem_chunk *) malloc(mem_size);

    ASSERT_EQ(chunk != NULL, true);

    int ret = chunk->init(mem_size, sizeof(long));
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(chunk->prev_lru, IDX_NULL);
    ASSERT_EQ(chunk->next_lru, IDX_NULL);
    ASSERT_EQ(chunk->next_chunk, IDX_NULL);

    ASSERT_EQ(chunk->full(), false);

    std::vector<long *> container;

    for (int i = 0; i < BLK_CNT; ++i) {
        int idx = chunk->malloc();
        ASSERT_TRUE(idx >= 0);

        long *l = cast_ptr(long, chunk->data + idx * chunk->block_size);
        *l = i;
        container.push_back(l);
    }

    ASSERT_EQ(chunk->full(), true);

    for (int i = 0; i < 10; ++i) {
        size_t offset = (char_ptr(container[i]) - chunk->data);
        ASSERT_TRUE(offset % chunk->block_size == 0);
        chunk->free(offset / chunk->block_size);
        container.erase(container.begin() + i);
    }

    for (int i = 100; i < 110; ++i) {
        int idx = chunk->malloc();
        ASSERT_TRUE(idx >= 0);

        long *l = cast_ptr(long, chunk->data + idx * chunk->block_size);
        *l = i;
        container.push_back(l);
    }

    ASSERT_EQ(chunk->full(), true);

    for (std::vector<long *>::iterator it = container.begin();
         it != container.end(); ++it) {
        std::cout << *(*it) << std::endl;

        size_t offset = (char_ptr(*it) - chunk->data);
        ASSERT_TRUE(offset % chunk->block_size == 0);
        chunk->free(offset / chunk->block_size);
    }

    free(chunk);
}

TEST(shm_mgr, buddy) {
    const size_t mem_size = buddy::calc_size(BUDDY_SIZE);
    char buffer[mem_size];
    char *pool = (char *) malloc(BUDDY_SIZE);
    buddy *b = buddy::create(buffer, sizeof(buffer), false, pool, BUDDY_SIZE, BUDDY_SIZE, 1);
    ASSERT_TRUE(b != NULL);

    int offset1 = IDX_NULL;
    int ret = b->malloc(4, offset1);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(offset1 != -1);

    int offset2 = IDX_NULL;
    ret = b->malloc(30, offset2);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(offset2 != -1);

    int offset3 IDX_NULL;
    ret = b->malloc(1, offset3);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(offset3 != -1);

    int offset4 = IDX_NULL;
    ret = b->malloc(6, offset4);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(offset4 != -1);

    int offset5 = IDX_NULL;
    ret = b->malloc(10, offset5);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(offset5 != -1);

    int invalid = IDX_NULL;
    ret = b->malloc(3, invalid);
    ASSERT_TRUE(ret == -ENOMEM);
    ASSERT_TRUE(invalid == -1);

    b->free(offset3);

    int valid IDX_NULL;
    ret = b->malloc(3, valid);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(valid != -1);

    b->free(offset1);
    b->free(offset2);
    b->free(offset4);
    b->free(offset5);
    b->free(valid);

    ret = b->malloc(62, valid);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(valid != -1);

    b->free(valid);


    buddy *b2 = buddy::create(buffer, sizeof(buffer), true, pool, BUDDY_SIZE, BUDDY_SIZE, 1);
    ASSERT_TRUE(b2 != NULL);

    ret = b2->malloc(30, valid);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(valid != -1);
    ret = b2->malloc(27, valid);
    ASSERT_TRUE(ret == 0);
    ASSERT_TRUE(valid != -1);

    free(pool);
}

TEST(shm_mgr, lru) {
    char *pool = (char *) malloc(SHM_MGR_CHUNK_COUNT * SHM_MGR_CHUNK_SIZE);
    ASSERT_TRUE(pool != NULL);

    lru l;
    l.head = IDX_NULL;
    l.tail = IDX_NULL;
    l.chunk_size = SHM_MGR_CHUNK_SIZE;
    l.pool = pool;

    ASSERT_TRUE(l.empty());
    ASSERT_EQ(l.pop(), IDX_NULL);

#define init_chunk(index) \
    ret = cast_ptr(mem_chunk, pool + SHM_MGR_CHUNK_SIZE * index)->init(SHM_MGR_CHUNK_SIZE, sizeof(long)); \
    ASSERT_EQ(ret, 0)

    int ret = 0;

    init_chunk(0);
    ret = l.renew(0);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 0);
    ASSERT_EQ(l.tail, 0);
    ASSERT_FALSE(l.empty());

    ret = l.renew(0);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 0);
    ASSERT_EQ(l.tail, 0);
    ASSERT_FALSE(l.empty());

    init_chunk(1);
    ret = l.renew(1);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(0)->next_lru, 1);
    ASSERT_EQ(l.__at(1)->prev_lru, 0);
    ASSERT_EQ(l.__at(1)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 0);
    ASSERT_EQ(l.tail, 1);
    ASSERT_FALSE(l.empty());

    ret = l.renew(1);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(0)->next_lru, 1);
    ASSERT_EQ(l.__at(1)->prev_lru, 0);
    ASSERT_EQ(l.__at(1)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 0);
    ASSERT_EQ(l.tail, 1);
    ASSERT_FALSE(l.empty());

    init_chunk(2);
    ret = l.renew(2);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(0)->next_lru, 1);
    ASSERT_EQ(l.__at(1)->prev_lru, 0);
    ASSERT_EQ(l.__at(1)->next_lru, 2);
    ASSERT_EQ(l.__at(2)->prev_lru, 1);
    ASSERT_EQ(l.__at(2)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 0);
    ASSERT_EQ(l.tail, 2);
    ASSERT_FALSE(l.empty());

    init_chunk(3);
    ret = l.renew(3);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(0)->next_lru, 1);
    ASSERT_EQ(l.__at(1)->prev_lru, 0);
    ASSERT_EQ(l.__at(1)->next_lru, 2);
    ASSERT_EQ(l.__at(2)->prev_lru, 1);
    ASSERT_EQ(l.__at(2)->next_lru, 3);
    ASSERT_EQ(l.__at(3)->prev_lru, 2);
    ASSERT_EQ(l.__at(3)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 0);
    ASSERT_EQ(l.tail, 3);
    ASSERT_FALSE(l.empty());

    init_chunk(4);
    ret = l.renew(4);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(0)->next_lru, 1);
    ASSERT_EQ(l.__at(1)->prev_lru, 0);
    ASSERT_EQ(l.__at(1)->next_lru, 2);
    ASSERT_EQ(l.__at(2)->prev_lru, 1);
    ASSERT_EQ(l.__at(2)->next_lru, 3);
    ASSERT_EQ(l.__at(3)->prev_lru, 2);
    ASSERT_EQ(l.__at(3)->next_lru, 4);
    ASSERT_EQ(l.__at(4)->prev_lru, 3);
    ASSERT_EQ(l.__at(4)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 0);
    ASSERT_EQ(l.tail, 4);
    ASSERT_FALSE(l.empty());

    ret = l.renew(1);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(0)->next_lru, 2);
    ASSERT_EQ(l.__at(2)->prev_lru, 0);
    ASSERT_EQ(l.__at(2)->next_lru, 3);
    ASSERT_EQ(l.__at(3)->prev_lru, 2);
    ASSERT_EQ(l.__at(3)->next_lru, 4);
    ASSERT_EQ(l.__at(4)->prev_lru, 3);
    ASSERT_EQ(l.__at(4)->next_lru, 1);
    ASSERT_EQ(l.__at(1)->prev_lru, 4);
    ASSERT_EQ(l.__at(1)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 0);
    ASSERT_EQ(l.tail, 1);
    ASSERT_FALSE(l.empty());

    ret = l.renew(0);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(l.__at(2)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(2)->next_lru, 3);
    ASSERT_EQ(l.__at(3)->prev_lru, 2);
    ASSERT_EQ(l.__at(3)->next_lru, 4);
    ASSERT_EQ(l.__at(4)->prev_lru, 3);
    ASSERT_EQ(l.__at(4)->next_lru, 1);
    ASSERT_EQ(l.__at(1)->prev_lru, 4);
    ASSERT_EQ(l.__at(1)->next_lru, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, 1);
    ASSERT_EQ(l.__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 2);
    ASSERT_EQ(l.tail, 0);
    ASSERT_FALSE(l.empty());

    ret = l.renew(0);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(l.__at(2)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(2)->next_lru, 3);
    ASSERT_EQ(l.__at(3)->prev_lru, 2);
    ASSERT_EQ(l.__at(3)->next_lru, 4);
    ASSERT_EQ(l.__at(4)->prev_lru, 3);
    ASSERT_EQ(l.__at(4)->next_lru, 1);
    ASSERT_EQ(l.__at(1)->prev_lru, 4);
    ASSERT_EQ(l.__at(1)->next_lru, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, 1);
    ASSERT_EQ(l.__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 2);
    ASSERT_EQ(l.tail, 0);
    ASSERT_FALSE(l.empty());

    ret = l.renew(4);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(l.__at(2)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(2)->next_lru, 3);
    ASSERT_EQ(l.__at(3)->prev_lru, 2);
    ASSERT_EQ(l.__at(3)->next_lru, 1);
    ASSERT_EQ(l.__at(1)->prev_lru, 3);
    ASSERT_EQ(l.__at(1)->next_lru, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, 1);
    ASSERT_EQ(l.__at(0)->next_lru, 4);
    ASSERT_EQ(l.__at(4)->prev_lru, 0);
    ASSERT_EQ(l.__at(4)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 2);
    ASSERT_EQ(l.tail, 4);
    ASSERT_FALSE(l.empty());

    int idx = l.pop();
    ASSERT_EQ(idx, 2);
    ASSERT_EQ(l.__at(2)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(2)->next_lru, IDX_NULL);
    ASSERT_EQ(l.__at(3)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(3)->next_lru, 1);
    ASSERT_EQ(l.__at(1)->prev_lru, 3);
    ASSERT_EQ(l.__at(1)->next_lru, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, 1);
    ASSERT_EQ(l.__at(0)->next_lru, 4);
    ASSERT_EQ(l.__at(4)->prev_lru, 0);
    ASSERT_EQ(l.__at(4)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 3);
    ASSERT_EQ(l.tail, 4);
    ASSERT_FALSE(l.empty());

    ret = l.renew(2);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(l.__at(3)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(3)->next_lru, 1);
    ASSERT_EQ(l.__at(1)->prev_lru, 3);
    ASSERT_EQ(l.__at(1)->next_lru, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, 1);
    ASSERT_EQ(l.__at(0)->next_lru, 4);
    ASSERT_EQ(l.__at(4)->prev_lru, 0);
    ASSERT_EQ(l.__at(4)->next_lru, 2);
    ASSERT_EQ(l.__at(2)->prev_lru, 4);
    ASSERT_EQ(l.__at(2)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 3);
    ASSERT_EQ(l.tail, 2);
    ASSERT_FALSE(l.empty());

    l.remove(3);
    ASSERT_EQ(l.__at(3)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(3)->next_lru, IDX_NULL);
    ASSERT_EQ(l.__at(1)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(1)->next_lru, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, 1);
    ASSERT_EQ(l.__at(0)->next_lru, 4);
    ASSERT_EQ(l.__at(4)->prev_lru, 0);
    ASSERT_EQ(l.__at(4)->next_lru, 2);
    ASSERT_EQ(l.__at(2)->prev_lru, 4);
    ASSERT_EQ(l.__at(2)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 1);
    ASSERT_EQ(l.tail, 2);
    ASSERT_FALSE(l.empty());

    l.remove(2);
    ASSERT_EQ(l.__at(2)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(2)->next_lru, IDX_NULL);
    ASSERT_EQ(l.__at(1)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(1)->next_lru, 0);
    ASSERT_EQ(l.__at(0)->prev_lru, 1);
    ASSERT_EQ(l.__at(0)->next_lru, 4);
    ASSERT_EQ(l.__at(4)->prev_lru, 0);
    ASSERT_EQ(l.__at(4)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 1);
    ASSERT_EQ(l.tail, 4);
    ASSERT_FALSE(l.empty());

    l.remove(0);
    ASSERT_EQ(l.__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(l.__at(1)->prev_lru, IDX_NULL);
    ASSERT_EQ(l.__at(1)->next_lru, 4);
    ASSERT_EQ(l.__at(4)->prev_lru, 1);
    ASSERT_EQ(l.__at(4)->next_lru, IDX_NULL);
    ASSERT_EQ(l.head, 1);
    ASSERT_EQ(l.tail, 4);
    ASSERT_FALSE(l.empty());

    lru *pl = &l;
    ASSERT_FALSE(pl->empty());

    ret = pl->renew(2);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(pl->__at(1)->prev_lru, IDX_NULL);
    ASSERT_EQ(pl->__at(1)->next_lru, 4);
    ASSERT_EQ(pl->__at(4)->prev_lru, 1);
    ASSERT_EQ(pl->__at(4)->next_lru, 2);
    ASSERT_EQ(pl->__at(2)->prev_lru, 4);
    ASSERT_EQ(pl->__at(2)->next_lru, IDX_NULL);
    ASSERT_EQ(pl->head, 1);
    ASSERT_EQ(pl->tail, 2);
    ASSERT_FALSE(pl->empty());

    idx = pl->pop();
    ASSERT_EQ(idx, 1);
    ASSERT_EQ(pl->__at(1)->prev_lru, IDX_NULL);
    ASSERT_EQ(pl->__at(1)->next_lru, IDX_NULL);
    ASSERT_EQ(pl->__at(4)->prev_lru, IDX_NULL);
    ASSERT_EQ(pl->__at(4)->next_lru, 2);
    ASSERT_EQ(pl->__at(2)->prev_lru, 4);
    ASSERT_EQ(pl->__at(2)->next_lru, IDX_NULL);
    ASSERT_EQ(pl->head, 4);
    ASSERT_EQ(pl->tail, 2);
    ASSERT_FALSE(pl->empty());

    idx = pl->pop();
    ASSERT_EQ(idx, 4);
    ASSERT_EQ(pl->__at(4)->prev_lru, IDX_NULL);
    ASSERT_EQ(pl->__at(4)->next_lru, IDX_NULL);
    ASSERT_EQ(pl->__at(2)->prev_lru, IDX_NULL);
    ASSERT_EQ(pl->__at(2)->next_lru, IDX_NULL);
    ASSERT_EQ(pl->head, 2);
    ASSERT_EQ(pl->tail, 2);
    ASSERT_FALSE(pl->empty());

    pl->remove(2);
    ASSERT_EQ(pl->__at(2)->prev_lru, IDX_NULL);
    ASSERT_EQ(pl->__at(2)->next_lru, IDX_NULL);
    ASSERT_EQ(pl->head, IDX_NULL);
    ASSERT_EQ(pl->tail, IDX_NULL);
    ASSERT_TRUE(pl->empty());

    ASSERT_EQ(l.head, IDX_NULL);
    ASSERT_EQ(l.tail, IDX_NULL);
    ASSERT_TRUE(l.empty());

    free(pool);

#undef init_chunk
}

TEST(shm_mgr, chunk_mgr) {
    size_t mgr_size = chunk_mgr::calc_size(sizeof(long) * 3 + 1);
    size_t mem_size = 0;
    mem_size += mgr_size;
    mem_size += (sizeof(long) * 3 + sizeof(mem_chunk)) * 3;

    char *addr = (char *) malloc(mem_size);
    ASSERT_TRUE(addr != NULL);
    char *pool = addr + mgr_size;

    chunk_mgr *mgr = chunk_mgr::create(addr, mgr_size, false, pool, mem_size - mgr_size,
                                       sizeof(long) * 3 + 1,sizeof(long) * 3 + sizeof(mem_chunk), 3);
    ASSERT_TRUE(mgr != NULL);

    int chunk_index = IDX_NULL;
    int block_index = IDX_NULL;
    int ret = mgr->malloc(sizeof(long), chunk_index, block_index);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(chunk_index, 0);
    ASSERT_EQ(block_index, 0);
    ASSERT_EQ(mgr->partial_buckets[sizeof(long)], 0);
    ASSERT_EQ(mgr->empty_buckets[sizeof(long)], IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.head, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.tail, IDX_NULL);

    ret = mgr->malloc(sizeof(long), chunk_index, block_index);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(chunk_index, 0);
    ASSERT_EQ(block_index, 1);
    ASSERT_EQ(mgr->partial_buckets[sizeof(long)], 0);
    ASSERT_EQ(mgr->empty_buckets[sizeof(long)], IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.head, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.tail, IDX_NULL);

    ret = mgr->malloc(sizeof(long), chunk_index, block_index);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(chunk_index, 0);
    ASSERT_EQ(block_index, 2);
    ASSERT_EQ(mgr->partial_buckets[sizeof(long)], IDX_NULL);
    ASSERT_EQ(mgr->empty_buckets[sizeof(long)], IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.head, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.tail, IDX_NULL);

    ret = mgr->malloc(sizeof(long), chunk_index, block_index);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(chunk_index, 1);
    ASSERT_EQ(block_index, 0);
    ASSERT_EQ(mgr->partial_buckets[sizeof(long)], 1);
    ASSERT_EQ(mgr->empty_buckets[sizeof(long)], IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.head, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.tail, IDX_NULL);

    mgr->free(0, 1);
    ASSERT_EQ(mgr->partial_buckets[sizeof(long)], 0);
    ASSERT_EQ(mgr->empty_buckets[sizeof(long)], IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_chunk, 1);
    ASSERT_EQ(mgr->__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.head, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.tail, IDX_NULL);

    mgr->free(1, 0);
    ASSERT_EQ(mgr->partial_buckets[sizeof(long)], 0);
    ASSERT_EQ(mgr->empty_buckets[sizeof(long)], 1);
    ASSERT_EQ(mgr->__at(0)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.head, 1);
    ASSERT_EQ(mgr->lru_cache.tail, 1);

    mgr->free(0, 0);
    mgr->free(0, 2);
    ASSERT_EQ(mgr->partial_buckets[sizeof(long)], IDX_NULL);
    ASSERT_EQ(mgr->empty_buckets[sizeof(long)], 0);
    ASSERT_EQ(mgr->__at(0)->next_chunk, 1);
    ASSERT_EQ(mgr->__at(0)->prev_lru, 1);
    ASSERT_EQ(mgr->__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->next_lru, 0);
    ASSERT_EQ(mgr->lru_cache.head, 1);
    ASSERT_EQ(mgr->lru_cache.tail, 0);

    ret = mgr->malloc(sizeof(long) * 3, chunk_index, block_index);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(chunk_index, 2);
    ASSERT_EQ(block_index, 0);
    ASSERT_EQ(mgr->partial_buckets[sizeof(long) * 3], IDX_NULL);
    ASSERT_EQ(mgr->empty_buckets[sizeof(long) * 3], IDX_NULL);
    ASSERT_EQ(mgr->__at(2)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(2)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(2)->next_lru, IDX_NULL);

    ret = mgr->malloc(sizeof(long) * 3, chunk_index, block_index);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(chunk_index, 1);
    ASSERT_EQ(block_index, 0);
    ASSERT_EQ(mgr->empty_buckets[sizeof(long)], 0);
    ASSERT_EQ(mgr->partial_buckets[sizeof(long)], IDX_NULL);
    ASSERT_EQ(mgr->empty_buckets[sizeof(long) * 3], IDX_NULL);
    ASSERT_EQ(mgr->partial_buckets[sizeof(long) * 3], IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(1)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.head, 0);
    ASSERT_EQ(mgr->lru_cache.tail, 0);

    ret = mgr->malloc(sizeof(long), chunk_index, block_index);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(chunk_index, 0);
    ASSERT_EQ(block_index, 2);
    ASSERT_EQ(mgr->empty_buckets[sizeof(long)], IDX_NULL);
    ASSERT_EQ(mgr->partial_buckets[sizeof(long)], 0);
    ASSERT_EQ(mgr->__at(0)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.head, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.tail, IDX_NULL);

    ret = mgr->malloc(sizeof(int), chunk_index, block_index);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(chunk_index, 0);
    ASSERT_EQ(block_index, 0);
    ASSERT_EQ(mgr->empty_buckets[sizeof(int)], IDX_NULL);
    ASSERT_EQ(mgr->partial_buckets[sizeof(int)], IDX_NULL);
    ASSERT_EQ(mgr->empty_buckets[sizeof(long)], IDX_NULL);
    ASSERT_EQ(mgr->partial_buckets[sizeof(long)], 0);
    ASSERT_EQ(mgr->__at(0)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.head, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.tail, IDX_NULL);

    ret = mgr->malloc(sizeof(int), chunk_index, block_index);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(chunk_index, 0);
    ASSERT_EQ(block_index, 1);
    ASSERT_EQ(mgr->empty_buckets[sizeof(int)], IDX_NULL);
    ASSERT_EQ(mgr->partial_buckets[sizeof(int)], IDX_NULL);
    ASSERT_EQ(mgr->empty_buckets[sizeof(long)], IDX_NULL);
    ASSERT_EQ(mgr->partial_buckets[sizeof(long)], IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(0)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.head, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.tail, IDX_NULL);

    ret = mgr->malloc(1, chunk_index, block_index);
    ASSERT_EQ(ret, -ENOMEM);

    chunk_mgr *mgr2 = chunk_mgr::create(addr, mgr_size, true, pool, mem_size - mgr_size,
                                        sizeof(long) * 3 + 1, sizeof(long) * 3 + sizeof(mem_chunk), 3);
    ASSERT_TRUE(mgr2 != NULL);

    ret = mgr2->malloc(1, chunk_index, block_index);
    ASSERT_EQ(ret, -ENOMEM);

    mgr2->free(2, 0);
    ASSERT_EQ(mgr2->partial_buckets[sizeof(long) * 3], IDX_NULL);
    ASSERT_EQ(mgr2->empty_buckets[sizeof(long) * 3], 2);
    ASSERT_EQ(mgr2->__at(2)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr2->__at(2)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr2->__at(2)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr2->lru_cache.head, 2);
    ASSERT_EQ(mgr2->lru_cache.tail, 2);
    ASSERT_EQ(mgr->partial_buckets[sizeof(long) * 3], IDX_NULL);
    ASSERT_EQ(mgr->empty_buckets[sizeof(long) * 3], 2);
    ASSERT_EQ(mgr->__at(2)->next_chunk, IDX_NULL);
    ASSERT_EQ(mgr->__at(2)->prev_lru, IDX_NULL);
    ASSERT_EQ(mgr->__at(2)->next_lru, IDX_NULL);
    ASSERT_EQ(mgr->lru_cache.head, 2);
    ASSERT_EQ(mgr->lru_cache.tail, 2);

    free(addr);
}

struct size24 {
    size_t a;
    size_t b;
    size_t c;
};

struct size1000 {
    char str[1000];
};

struct size1024 {
    char str1[256];
    char str2[256];
    char str3[256];
    char str4[256];
};

struct size1032 {
    char str[1024];
    size_t i;
};

struct size2064 {
    char str1[1024];
    char str2[1024];
    int a;
    int b;
    int c;
    int d;
};

TEST(shm_mgr, shm_mgr) {
    shm_register_singleton(ST_MIN, sizeof(size1032));

    shm_mgr *mgr = shm_mgr::create(SHM_MGR_KEY, false, SHM_MGR_CHUNK_SIZE,
                                   SHM_MGR_CHUNK_COUNT, SHM_MGR_HEAP_SIZE);
    ASSERT_TRUE(mgr != NULL);

    size24 *s24_1 = NULL;
    shm_ptr<void> ps24_1 = mgr->malloc(sizeof(*s24_1));
    s24_1 = static_cast<size24 *>(mgr->mid2ptr(ps24_1.mid));
    ASSERT_TRUE(ps24_1 && s24_1);

    size24 *s24_2 = NULL;
    shm_ptr<void> ps24_2 = mgr->malloc(sizeof(*s24_2));
    s24_2 = static_cast<size24 *>(mgr->mid2ptr(ps24_2.mid));
    ASSERT_TRUE(ps24_2 && s24_2);

    mgr->free(ps24_1);
    mgr->free(ps24_2);

    ASSERT_TRUE(sizeof(size1000) == mgr->max_block_size);
    shm_ptr<void> s1000_blocks[SHM_MGR_CHUNK_COUNT];
    for (int i = 0; i < SHM_MGR_CHUNK_COUNT; ++i) {
        size1000 *tmp = NULL;
        s1000_blocks[i] = mgr->malloc(sizeof(*tmp));
        tmp = static_cast<size1000 *>(mgr->mid2ptr(s1000_blocks[i].mid));
        ASSERT_TRUE(s1000_blocks[i] && tmp);
    }

    mgr->free(s1000_blocks[0]);

    shm_ptr<void> s24_blocks[50];
    int avail_count = mgr->max_block_size / sizeof(size24);
    for (int i = 0; i < avail_count; ++i) {
        size24 *tmp = NULL;
        s24_blocks[i] = mgr->malloc(sizeof(*tmp));
        tmp = static_cast<size24 *>(mgr->mid2ptr(s24_blocks[i].mid));
        ASSERT_TRUE(s24_blocks[i] && tmp);
    }

    shm_ptr<void> s1024_blocks[8];
    for (int i = 0; i < 8; ++i) {
        size1024 *tmp = NULL;
        s1024_blocks[i] = mgr->malloc(sizeof(*tmp));
        tmp = static_cast<size1024 *>(mgr->mid2ptr(s1024_blocks[i].mid));
        ASSERT_TRUE(s1024_blocks[i] && tmp);
    }

    size1024 *s1024_inval = NULL;
    shm_ptr<void> ps1024_inval = mgr->malloc(sizeof(*s1024_inval));
    s1024_inval = static_cast<size1024 *>(mgr->mid2ptr(ps1024_inval.mid));
    ASSERT_TRUE(!ps1024_inval && !s1024_inval);

    mgr->free(s1024_blocks[0]);
    mgr->free(s1024_blocks[1]);

    size2064 *s2064 = NULL;
    shm_ptr<void> ps2064 = mgr->malloc(sizeof(*s2064));
    s2064 = static_cast<size2064 *>(mgr->mid2ptr(ps2064.mid));
    ASSERT_TRUE(ps2064 && s2064);


    s2064->a = 10;
    s2064->b = 20;
    s2064->c = 30;
    snprintf(s2064->str1, sizeof(s2064->str1), "%s", "Talk is cheap.");
    snprintf(s2064->str2, sizeof(s2064->str2), "%s", "Show me the code.");

    size24 *s24 = cast_ptr(size24, mgr->mid2ptr(s24_blocks[10].mid));
    s24->a = 1;
    s24->b = 2;
    s24->c = 3;

    size1000 *s1000 = cast_ptr(size1000, mgr->mid2ptr(s1000_blocks[20].mid));
    snprintf(s1000->str, sizeof(s1000->str), "%s", "silly kelvin");

    size1032 *single_1032 = cast_ptr(size1032, mgr->get_singleton(ST_MIN));
    ASSERT_TRUE(single_1032 != NULL);
    single_1032->i = 77;
    snprintf(single_1032->str, sizeof(single_1032->str), "%s", "silly kelvin");


    shm_mgr *mgr2 = shm_mgr::create(SHM_MGR_KEY, true, SHM_MGR_CHUNK_SIZE,
                                    SHM_MGR_CHUNK_COUNT, SHM_MGR_HEAP_SIZE);

    s2064 = cast_ptr(size2064, mgr2->mid2ptr(ps2064.mid));
    ASSERT_TRUE(s2064 != NULL);
    ASSERT_TRUE(s2064->a == 10);
    ASSERT_TRUE(s2064->b == 20);
    ASSERT_TRUE(s2064->c == 30);
    ASSERT_STREQ(s2064->str1, "Talk is cheap.");
    ASSERT_STREQ(s2064->str2, "Show me the code.");

    s24 = cast_ptr(size24, mgr2->mid2ptr(s24_blocks[10].mid));
    ASSERT_TRUE(s24 != NULL);
    ASSERT_TRUE(s24->a == 1);
    ASSERT_TRUE(s24->b == 2);
    ASSERT_TRUE(s24->c == 3);

    s1000 = cast_ptr(size1000, mgr2->mid2ptr(s1000_blocks[20].mid));
    ASSERT_TRUE(s1000 != NULL);
    ASSERT_STREQ(s1000->str, "silly kelvin");

    size1032 *single_1032_2 = cast_ptr(size1032, mgr->get_singleton(ST_MIN));
    ASSERT_TRUE(single_1032_2 != NULL);
    ASSERT_TRUE(single_1032_2->i == 77);
    ASSERT_STREQ(single_1032_2->str, "silly kelvin");
}
