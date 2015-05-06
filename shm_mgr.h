#ifndef SHM_MGR_H
#define SHM_MGR_H

#include <stddef.h>
#include "assert_helper.h"
#include "types.h"
#include "shm_seg.h"

#define CAST_PTR(type, ptr) static_cast<type *>(ptr)
#define VOID_PTR(ptr) CAST_PTR(void, (ptr))
#define CHAR_PTR(ptr) CAST_PTR(char, (ptr))

namespace sk {

static const int MAGIC            = 0xC0DEFEED;  // "code feed" :-)
static const int ALIGN_SIZE       = 8;           // memory align size, 8 bytes
static const int ALIGN_MASK       = ALIGN_SIZE - 1;
static const int SMALL_CHUNK_SIZE = 1024 * 1024; // 1MB

enum singleton_type {
    ST_MIN = 0,

    // ...

    ST_MAX
};

int register_singleton(int id, size_t size);

namespace detail {

template<typename K>
size_t hashcode(const K& k) {
    return static_cast<size_t>(k);
}

template<typename K, typename V>
struct hash_node {
    K k;
    V v;

    int next;
};

/**
 * K: hash key
 * V: hash value
 * D: allow duplication or not
 * F: the function to calculate hashcode for K
 */
template<typename K, typename V, bool D = true, size_t(*F)(const K& k) = hashcode>
struct hash {
    typedef hash_node<K, V> node;

    int max_node_count;
    int curr_node_count;
    int hash_size;
    int free_node_head;
    int *buckets;
    node *nodes;

    static hash *create(key_t key, bool resume, int max_node_count, int hash_size = 0) {
        if (hash_size == 0)
            hash_size = max_node_count;

        size_t shm_size = sizeof(hash) + max_node_count * sizeof(node) + hash_size * sizeof(int);

        sk::shm_seg seg;
        int ret = seg.init(key, shm_size, resume);
        if (ret != 0) {
            ERR("cannot create hash map, key<%d>, size<%lu>.", key, shm_size);
            return NULL;
        }

        hash *self = NULL;
        if (resume) {
            self = static_cast<hash *>(seg.address());
        } else {
            self = static_cast<hash *>(seg.malloc(sizeof(hash)));
        }

        if (!self) {
            ERR("memory error.");
            seg.free();
            return NULL;
        }

        if (resume) {
            char *base_addr = static_cast<char *>(seg.address());
            self->buckets = static_cast<int *>(static_cast<void *>(base_addr + sizeof(hash)));
            self->nodes = static_cast<node *>(static_cast<void *>(base_addr + sizeof(hash) + sizeof(int) * hash_size));
        } else {
            self->buckets = static_cast<int *>(seg.malloc(sizeof(int) * hash_size));
            if (!self->buckets) {
                ERR("memory error.");
                seg.free();
                return NULL;
            }

            self->nodes = static_cast<node *>(seg.malloc(sizeof(node) * max_node_count));
            if (!self->nodes) {
                ERR("memory error.");
                seg.free();
                return NULL;
            }

            assert_noeffect(seg.free_size == 0);

            self->max_node_count = max_node_count;
            self->curr_node_count = 0;
            self->hash_size = hash_size;

            for (int i = 0; i < hash_size; ++i)
                self->buckets[i] = IDX_NULL;

            for (int i = 0; i < max_node_count; ++i)
                self->nodes[i].next = i + 1;

            self->nodes[max_node_count - 1].next = IDX_NULL;

            self->free_node_head = 0;
        }

        return self;
    }

    bool full() const {
        return curr_node_count >= max_node_count;
    }

    bool empty() const {
        return curr_node_count <= 0;
    }

    /**
     * @brief search
     * @param k
     * @return the first node holds key k, NULL if not found
     *
     * NOTE: this function will always return the first matching node,
     *       even if there could be several nodes all hold key k.
     */
    node *search(const K& k) {
        if (empty())
            return NULL;

        size_t hashcode = F(k);
        int bucket_idx = hashcode % hash_size;

        int idx = buckets[bucket_idx];
        while (idx != IDX_NULL) {
            node& n = nodes[idx];
            if (n.k == k)
                return &n;

            idx = n.next;
        }

        return NULL;
    }

    /**
     * @brief find
     * @param k
     * @return the first found value
     *
     * NOTE: like search, only the first value will be returned
     */
    V* find(const K& k) {
        if (empty())
            return NULL;

        node *n = search(k);
        if (!n)
            return NULL;

        return &n->v;
    }

    int insert(const K& k, const V& v) {
        if (full())
            return -ENOMEM;

        /*
         * if duplication is not allowed
         */
        if (!D) {
            node *n = search(k);
            if (n) {
                n->v = v;
                return 0;
            }
        }

        size_t hashcode = F(k);
        int idx = hashcode % hash_size;

        if (buckets[idx] != IDX_NULL)
            DBG("hash collision detected, idx<%d>, hashcode<%ld>.", idx, hashcode);

        int node_idx = free_node_head;
        node& n = nodes[node_idx];

        free_node_head = n.next;

        n.k = k;
        n.v = v;
        n.next = buckets[idx];
        buckets[idx] = node_idx;

        ++curr_node_count;

        return 0;
    }

    /**
     * @brief erase
     * @param k
     * @return success or not, success: 0
     *
     * NOTE: like search, it will always operate on the first found node
     */
    int erase(const K& k) {
        if (empty())
            return 0;

        size_t hashcode = F(k);
        int bucket_idx = hashcode % hash_size;

        bool found = false;
        int prev = IDX_NULL;
        int idx = buckets[bucket_idx];
        while (idx != IDX_NULL) {
            node& n = nodes[idx];
            if (n.k == k){
                found = true;
                break;
            }

            prev = idx;
            idx = n.next;
        }

        if (!found)
            return 0;

        if (prev == IDX_NULL) {
            assert_noeffect(buckets[bucket_idx] == idx);
            buckets[bucket_idx] = nodes[idx].next;
        } else {
            assert_noeffect(nodes[prev].next == idx);
            nodes[prev].next = nodes[idx].next;
        }

        nodes[idx].next = free_node_head;
        free_node_head = idx;
        --curr_node_count;

        return 0;
    }

    /**
     * @brief erase
     * @param k
     * @param v
     * @return success or not, success: 0
     *
     * NOTE: this function will erase the node whose key and value
     *       both exactly match the given parameters
     *
     * TODO: this function has much same code with erase(const K& k),
     *       consider to refactor it
     */
    int erase(const K& k, const V& v) {
        if (!D)
            return erase(k);

        size_t hashcode = F(k);
        int bucket_idx = hashcode % hash_size;

        bool found = false;
        int prev = IDX_NULL;
        int idx = buckets[bucket_idx];
        while (idx != IDX_NULL) {
            node& n = nodes[idx];
            if (n.k == k && n.v == v){
                found = true;
                break;
            }

            prev = idx;
            idx = n.next;
        }

        if (!found)
            return 0;

        if (prev == IDX_NULL) {
            assert_noeffect(buckets[bucket_idx] == idx);
            buckets[bucket_idx] = nodes[idx].next;
        } else {
            assert_noeffect(nodes[prev].next == idx);
            nodes[prev].next = nodes[idx].next;
        }

        nodes[idx].next = free_node_head;
        free_node_head = idx;
        --curr_node_count;

        return 0;
    }
};

template<typename T>
struct stack {
    int pos;
    int total_count;
    char data[0];

    static stack *create(key_t key, bool resume, int max_count) {
        size_t data_size = max_count * sizeof(T);
        size_t shm_size = sizeof(stack) + data_size;
        sk::shm_seg seg;
        int ret = seg.init(key, shm_size, resume);
        if (ret != 0) {
            ERR("cannot create list, key<%d>, size<%lu>.", key, shm_size);
            return NULL;
        }

        /*
         * no matter in resume mode or not, we cast the entire memory block to stack
         */
        stack *self = NULL;
        self = static_cast<stack *>(seg.address());

        if (!self) {
            ERR("memory error.");
            seg.free();
            return NULL;
        }

        if (!resume) {
            self->pos = 0;
            self->total_count = max_count;
            memset(self->data, 0x00, data_size);
        }

        return self;
    }

    bool empty() const {
        return pos <= 0;
    }

    bool full() const {
        return pos >= total_count;
    }

    int push(const T& t) {
        if (full())
            return -ENOMEM;

        T *ret = emplace();
        assert_retval(ret, -EINVAL);

        *ret = t;

        return 0;
    }

    T *emplace() {
        if (full())
            return NULL;

        T *t = static_cast<T *>(static_cast<void *>(data + sizeof(T) * pos));
        ++pos;

        return t;
    }

    T *pop() {
        if (empty())
            return NULL;

        --pos;
        return static_cast<T *>(static_cast<void *>(data + sizeof(T) * pos));
    }
};

struct mem_chunk {
    int magic;          // for memory overflow verification
    int free_head;      // free head index
    int free_count;     // free block count
    int total_count;    // total block count
    size_t block_size;  // block size
    char data[0];

    // void set_flag(int flag) {
    //     if (flags & flag)
    //         DBG("flag<%x> already exists.", flag);
    //     flags |= flag;
    // }

    // bool has_flag(int flag) const {
    //     return flags & flag;
    // }

    /**
     * @brief init
     * @param chunk_size: see the graph below
     * @param block_size: see the graph below
     * @return 0 if successfully inited
     *
     * NOTE: a chunk may contain many blocks, at lease 1 block
     *
     *    +-----------+   ---
     *    | mem_chunk |    |      block_size
     *    +-----------+    |         ---
     *    |  block 0  |    |          |
     *    +-----------+ chunk_size   ---
     *    |    ...    |    |
     *    +-----------+    |
     *    |  block N  |    |
     *    +-----------+   ---
     */
    int init(size_t chunk_size, size_t block_size) {
        assert_retval(chunk_size >= sizeof(mem_chunk) + block_size, -EINVAL);
        assert_retval(block_size % ALIGN_SIZE == 0, -EINVAL);
        assert_retval(block_size >= sizeof(int), -EINVAL);

        magic = MAGIC;
        // flags = 0;
        free_head = 0;
        free_count = (chunk_size - sizeof(mem_chunk)) / block_size;
        total_count = free_count;
        this->block_size = block_size;
        memset(data, 0x00, chunk_size - sizeof(mem_chunk));

        // link the free blocks
        int *n = NULL;
        for (int i = 0; i < free_count; ++i) {
            n = static_cast<int *>(static_cast<void *>(data + i * block_size));
            *n = i + 1;
        }
        *n = IDX_NULL; // the last block

        return 0;
    }

    bool full() const {
        assert_retval(magic == MAGIC, true); // mark the block as full if overflowed

        return free_count <= 0;
    }

    bool empty() const {
        assert_retval(magic == MAGIC, false);

        return free_count >= total_count;
    }

    void *malloc() {
        if (full())
            return NULL;

        void *tmp = static_cast<void *>(data + free_head * block_size);
        free_head = *(static_cast<int *>(tmp));
        --free_count;

        return tmp;
    }

    /*
     * NOTE: the offset here is measured from field data
     */
    void free(size_t offset) {
        assert_retnone(magic == MAGIC);
        assert_retnone(offset <= block_size * (total_count - 1));
        assert_retnone(offset % block_size == 0);

        int idx = offset / block_size;

        *(static_cast<int *>(static_cast<void *>(data + offset))) = free_head;
        free_head = idx;
        ++free_count;
    }
};

struct buddy {
    u32 size;
    /*
     * NOTE: as the size stored here is always the power of 2,
     *       so we can use only one byte to store the exponent.
     *
     *       however, if we only store the exponent, we have
     *       to calculate the actual size every time we need to
     *       use.
     *
     *       so, it should be determined by usage situation: if
     *       you want to save more memory, or be faster.
     */
    u32 longest[0];

    static inline int __left_leaf(int index) {
        return index * 2 + 1;
    }

    static inline int __right_leaf(int index) {
        return index * 2 + 2;
    }

    static inline int __parent(int index) {
        return (index + 1) / 2 - 1;
    }

    static inline bool __power_of_two(u32 num) {
        return !(num & (num - 1));
    }

    static inline u32 __fix_size(u32 size) {
        if (__power_of_two(size))
            return size;

        size |= size >> 1;
        size |= size >> 2;
        size |= size >> 4;
        size |= size >> 8;
        size |= size >> 16;
        return size + 1;
    }

    static buddy *create(key_t key, bool resume, u32 size) {
        assert_retval(size >= 1, NULL);

        u32 old_size = size;
        size = __fix_size(size);
        DBG("create buddy with size<%u>, after fixed<%u>.", old_size, size);

        size_t shm_size = 2 * size * sizeof(u32);

        sk::shm_seg seg;
        int ret = seg.init(key, shm_size, resume);
        if (ret != 0) {
            ERR("cannot create buddy, key<%d>, size<%lu>.", key, shm_size);
            return NULL;
        }

        /*
         * no matter in resume mode or not, we cast the entire memory block to buddy
         */
        buddy *self = NULL;
        self = static_cast<buddy *>(seg.address());

        if (!self) {
            ERR("memory error.");
            seg.free();
            return NULL;
        }

        if (!resume) {
            self->size = size;

            u32 node_size = size * 2;
            for (u32 i = 0; i < 2 * size - 1; ++i) {
                if (__power_of_two(i + 1))
                    node_size /= 2;
                self->longest[i] = node_size;
            }
        }

        return self;
    }

    inline u32 __max_child(int index) {
        u32 left  = longest[__left_leaf(index)];
        u32 right = longest[__right_leaf(index)];
        return left > right ? left : right;
    }

    int malloc(u32 size) {
        if (size <= 0)
            size = 1;

        size = __fix_size(size);

        u32 node_size;
        int index = 0;
        int offset = 0;

        if (longest[index] < size)
            return -1;

        for (node_size = this->size; node_size != size; node_size /= 2) {
            int left  = __left_leaf(index);
            int right = __right_leaf(index);
            int min   = left;
            int max   = right;

            if (longest[min] > longest[max]) {
                min = right;
                max = left;
            }

            if (longest[min] >= size)
                index = min;
            else
                index = max;
        }

        longest[index] = 0;

        offset = (index + 1) * node_size - this->size;

        while (index) {
            index = __parent(index);
            longest[index] = __max_child(index);
        }

        return offset;
    }

    void free(int offset) {
        assert_retnone(offset >= 0 && (u32) offset < size);

        u32 node_size = 1;
        int index = offset + size - 1;

        for (; longest[index]; index = __parent(index)) {
            node_size *= 2;
            if (index == 0)
                return;
        }

        longest[index] = node_size;

        while (index) {
            index = __parent(index);
            node_size *= 2;

            u32 left  = longest[__left_leaf(index)];
            u32 right = longest[__right_leaf(index)];

            if (left + right == node_size)
                longest[index] = node_size;
            else
                longest[index] = left > right ? left : right;
        }
    }
};

} // namespace detail

struct shm_mgr {
    typedef detail::hash<size_t, shm_ptr, true, detail::hashcode> size_ptr_hash;
    typedef detail::stack<shm_ptr> stack;
    typedef detail::mem_chunk mem_chunk;
    typedef detail::buddy heap_allocator;

    size_t chunk_size;
    /*
     * a block is an area inside a chunk, so:
     *     [max block size] = [chunk size] - [chunk header size]
     */
    size_t max_block_size;

    size_t heap_total_size;
    size_t heap_unit_size;

    shm_ptr singletons[ST_MAX];

    /*
     * the base address of entire mem pool
     * also, it is the head of chunk pool
     */
    char *pool;

    /*
     * pool_head_ptr is the head ptr of pool:
     *     [pool head] = [pool addr] - [addr of this]
     *
     * pool_end_ptr is the end ptr of pool:
     *     [pool end] = [actual total shm size]
     *
     * NOTE: pool_end_ptr is NOT in the pool!!!
     */
    shm_ptr pool_head_ptr;
    shm_ptr pool_end_ptr;

    /*
     * the pool will be divided into two parts:
     *     1. chunk pool
     *     2. heap
     *
     * chunk_end is the "realtime" end of chunk pool
     * heap_head is the fixed head of heap
     *
     * chunk_end can grow or shrink, if it meets heap_head,
     * then there is no more available chunk, but, it does
     * NOT mean there is no more space on heap
     */
    size_t chunk_end;
    size_t heap_head;

    size_ptr_hash *free_chunk_hash;
    stack *empty_chunk_stack;

    heap_allocator *heap;

    static inline bool __power_of_two(size_t num) {
        return !(num & (num - 1));
    }

    static inline size_t __fix_size(size_t size) {
        if (__power_of_two(size))
            return size;

        size |= size >> 1;
        size |= size >> 2;
        size |= size >> 4;
        size |= size >> 8;
        size |= size >> 16;
        size |= size >> 32;
        return size + 1;
    }

    static shm_mgr *create(key_t main_key, key_t aux_key1, key_t aux_key2, key_t aux_key3,
                           bool resume, size_t chunk_size, int chunk_count, size_t heap_size) {
        chunk_size = __align_size(chunk_size);

        size_t max_block_size = chunk_size - sizeof(mem_chunk);
        // for blocks with size > max_block_size will go into heap
        size_t heap_unit_size = __align_size(max_block_size);
        size_t heap_unit_count = heap_size / heap_unit_size;

        // heap unit count must be 2^n
        heap_unit_count = __fix_size(heap_unit_count);
        heap_size  = heap_unit_count * heap_unit_size;

        // TODO: calculate singleton sizes here
        size_t singleton_size = 8;

        size_t shm_size = 0;
        shm_size += sizeof(shm_mgr);
        shm_size += singleton_size;
        shm_size += chunk_size * chunk_count;
        shm_size += heap_size;

        sk::shm_seg seg;
        int ret = seg.init(main_key, shm_size, resume);
        if (ret != 0) {
            ERR("cannot create shm mgr, key<%d>, size<%lu>.", main_key, shm_size);
            return NULL;
        }

        shm_mgr *self = NULL;
        if (resume)
            self = static_cast<shm_mgr *>(seg.address());
        else
            self = static_cast<shm_mgr *>(seg.malloc(sizeof(shm_mgr)));

        if (!self) {
            ERR("memory error.");
            seg.free();
            return NULL;
        }

        char *base_addr = static_cast<char *>(static_cast<void *>(self));
        self->pool = base_addr + sizeof(*self) + singleton_size;

        if (!resume) {
            self->chunk_size = chunk_size;
            self->max_block_size = chunk_size - sizeof(mem_chunk);

            self->heap_total_size = heap_size;
            self->heap_unit_size = heap_unit_size;

            // TODO: singletons here

            self->pool_head_ptr = self->pool - base_addr;
            self->pool_end_ptr  = shm_size;

            self->chunk_end = 0;
            self->heap_head = chunk_size * chunk_count;
        }

        // 1. init hash
        {
            int node_count = chunk_count;
            int hash_size  = chunk_size - sizeof(mem_chunk); // max block size
            self->free_chunk_hash = size_ptr_hash::create(aux_key1, resume, node_count, hash_size);
            if (!self->free_chunk_hash) {
                ERR("cannot create free chunk hash.");
                seg.free();
                return NULL;
            }
        }

        // 2. init stack
        {
            int node_count = chunk_count;
            self->empty_chunk_stack = stack::create(aux_key2, resume, node_count);
            if (!self->empty_chunk_stack) {
                ERR("cannot create empty chunk stack.");
                seg.free();
                return NULL;
            }
        }

        // 3. init heap
        {
            int unit_count = static_cast<int>(heap_unit_count);
            self->heap = heap_allocator::create(aux_key3, resume, unit_count);
            if (!self->heap) {
                ERR("cannot create heap.");
                seg.free();
                return NULL;
            }
        }

        return self;
    }

    static size_t __align_size(size_t size) {
        bool has_lo = false;
        if (size & ALIGN_MASK)
            has_lo = true;

        size &= ~ALIGN_MASK;
        if (has_lo)
            size += ALIGN_SIZE;

        return size;
    }

    void *ptr2ptr(shm_ptr ptr) {
        assert_retval(ptr != SHM_NULL, NULL);

        return static_cast<void *>(static_cast<char *>(static_cast<void *>(this)) + ptr);
    }

    shm_ptr ptr2ptr(void *ptr) {
        ptrdiff_t offset = static_cast<char *>(ptr) - static_cast<char *>(static_cast<void *>(this));
        assert_retval(offset >= 0, SHM_NULL);

        return offset;
    }

    mem_chunk *__ptr2chunk(shm_ptr ptr) {
        void *p = ptr2ptr(ptr);
        assert_retval(p, NULL);

        // TODO: here may add alignment verification:
        // if ptr is in small chunk allocation area, check if:
        //    (ptr - base_addr) % small_chunk_size == 0

        return static_cast<mem_chunk *>(p);
    }

    shm_ptr __chunk2ptr(mem_chunk *chunk) {
        assert_retval(chunk, SHM_NULL);

        return ptr2ptr(static_cast<void *>(chunk));
    }

    shm_ptr __malloc_from_chunk_pool(size_t mem_size, void *&ptr) {
        mem_chunk *chunk = NULL;

        do {
            shm_ptr *chunk_ptr = free_chunk_hash->find(mem_size);

            // 1. there exists free chunk with same size
            if (chunk_ptr) {
                chunk = __ptr2chunk(*chunk_ptr);
                break;
            }

            // 2. no free chunk exists, but there is empty chunk
            if (!empty_chunk_stack->empty()) {
                chunk_ptr = empty_chunk_stack->pop();
                assert_retval(chunk_ptr, SHM_NULL);

                chunk = __ptr2chunk(*chunk_ptr);
                chunk->init(chunk_size, mem_size);

                // TODO: here if the chunk has only one block,
                // then there is no need to insert it into the
                // hash, as we will remove it from the hash later
                // this condition also applies to item 3
                int ret = free_chunk_hash->insert(mem_size, *chunk_ptr);
                assert_retval(ret == 0, SHM_NULL);

                break;
            }

            // 3. there is available chunk in pool
            if (chunk_end + chunk_size <= heap_head) {
                void *addr = static_cast<void *>(pool + chunk_end);
                chunk = static_cast<mem_chunk *>(addr);
                chunk->init(chunk_size, mem_size);

                int ret = free_chunk_hash->insert(mem_size, ptr2ptr(addr));
                assert_retval(ret == 0, SHM_NULL);

                chunk_end += chunk_size;
                break;
            }

            // 4. no empty chunk, and also chunk pool has used up
            //    we hope it will never get here :(
            ERR("chunk pool has been used up!!!");
            return SHM_NULL;

        } while (0);

        assert_retval(chunk, SHM_NULL);

        // actually, it should never get here
        if (chunk->full()) {
            assert_noeffect(0);

            // however, if it gets here, we still can handle:
            // remove the full chunk, and call this function recursively
            free_chunk_hash->erase(mem_size);
            return __malloc_from_chunk_pool(mem_size, ptr);
        }

        ptr = chunk->malloc();
        assert_retval(ptr, SHM_NULL);

        if (chunk->full())
            free_chunk_hash->erase(mem_size);

        return ptr2ptr(ptr);
    }

    shm_ptr __malloc_from_heap(size_t mem_size, void *&ptr) {
        u32 unit_count = mem_size / heap_unit_size;
        if (mem_size % heap_unit_size != 0)
            unit_count += 1;

        int offset = heap->malloc(unit_count);

        // there is no more memory
        // we hope it will never get here :(
        if (offset < 0) {
            ERR("no more space on heap!!!");
            return SHM_NULL;
        }

        ptr = static_cast<void *>(pool + heap_head + heap_unit_size * offset);
        return ptr2ptr(ptr);
    }

    template<typename T>
    shm_ptr malloc(T *&t) {
        size_t mem_size = __align_size(sizeof(T));
        bool use_chunk = mem_size <= max_block_size;

        shm_ptr ptr = SHM_NULL;
        void *raw_ptr = NULL;

        if (use_chunk) {
            ptr = __malloc_from_chunk_pool(mem_size, raw_ptr);
            if (ptr == SHM_NULL) {
                // TODO: no more space in chunk pool, consider allocate one on heap
                return SHM_NULL;
            }

            assert_noeffect(raw_ptr);

            t = static_cast<T *>(raw_ptr);
            return ptr;
        }

        DBG("a big memory block<%lu>, allocate it on heap.", mem_size);

        ptr = __malloc_from_heap(mem_size, raw_ptr);
        if (ptr == SHM_NULL) // no more memory :(
            return SHM_NULL;

        assert_noeffect(raw_ptr);
        t = static_cast<T *>(raw_ptr);
        return ptr;
    }

    void __free_from_chunk_pool(size_t offset) {
        size_t chunk_offset = (offset / chunk_size) * chunk_size;
        assert_retnone(offset >= chunk_offset + sizeof(mem_chunk));

        size_t offset_in_chunk = offset - chunk_offset - sizeof(mem_chunk);

        mem_chunk *chunk = static_cast<mem_chunk *>(static_cast<void *>(pool + chunk_offset));

        bool full_before_free = chunk->full();

        chunk->free(offset_in_chunk);

        bool empty_after_free = chunk->empty();

        shm_ptr chunk_ptr = __chunk2ptr(chunk);

        if (full_before_free && empty_after_free) {
            int ret = empty_chunk_stack->push(chunk_ptr);
            assert_retnone(ret == 0);
        } else if (full_before_free) {
            int ret = free_chunk_hash->insert(chunk->block_size, chunk_ptr);
            assert_retnone(ret == 0);
        } else if (empty_after_free) {
            int ret = free_chunk_hash->erase(chunk->block_size, chunk_ptr);
            assert_retnone(ret == 0);

            ret = empty_chunk_stack->push(chunk_ptr);
            assert_retnone(ret == 0);
        } else {
            // not full before free, also not empty after free, it
            // will still lay in free_chunk_hash, so we do nothing here
        }
    }

    void __free_from_heap(size_t offset) {
        assert_retnone(offset >= heap_head);

        size_t heap_offset = offset - heap_head;
        assert_retnone(heap_offset % heap_unit_size == 0);

        int heap_unit_offset = heap_offset / heap_unit_size;
        heap->free(heap_unit_offset);
    }

    void free(shm_ptr ptr) {
        /*
         * NOTE: the condition between ptr and pool_end_ptr is <, not <=
         */
        assert_retnone(ptr >= pool_head_ptr && ptr < pool_end_ptr);

        size_t offset = ptr - pool_head_ptr;
        if (offset < chunk_end)
            return __free_from_chunk_pool(offset);

        return __free_from_heap(offset);
    }

    void free(void *ptr) {
        free(ptr2ptr(ptr));
    }
};

} // namespace sk

#endif // SHM_MGR_H
