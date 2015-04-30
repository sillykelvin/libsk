#ifndef SHM_MGR_H
#define SHM_MGR_H

#include <stddef.h>
#include "assert_helper.h"
#include "types.h"
#include "shm_seg.h"

#define CAST_PTR(type, ptr) static_cast<type *>(ptr)
#define CHAR_PTR(ptr) CAST_PTR(char, ptr)
#define VOID_PTR(ptr) CAST_PTR(void, ptr)

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

        stack *self = NULL;
        if (resume) {
            self = static_cast<stack *>(seg.address());
        } else {
            self = static_cast<stack *>(seg.malloc(sizeof(stack)));
        }

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

    void free(void *p) {
        assert_retnone(magic == MAGIC);
        assert_retnone(p);
        assert_retnone(p >= data);

        size_t offset = static_cast<char *>(p) - static_cast<char *>(data);
        assert_retnone(offset % block_size == 0);

        int idx = offset / block_size;

        *(static_cast<int *>(p)) = free_head;
        free_head = idx;
        ++free_count;
    }
};

} // namespace detail

struct shm_mgr {
    typedef detail::hash<size_t, shm_ptr, true, detail::hashcode> size_ptr_hash;
    typedef detail::mem_chunk mem_chunk;

    int small_chunk_size;
    int max_small_block_size;

    char *pool;
    shm_ptr singletons[ST_MAX];

    // if lo_bound meets hi_bound, then there is no available fresh memory
    size_t lo_bound; // the low  addr bound of free memory
    size_t hi_bound; // the high addr bound of free memory

    size_ptr_hash *free_chunk_hash;
    size_ptr_hash *used_chunk_hash;

    static shm_mgr *create(key_t main_key, key_t aux_key, bool resume,
                           int small_chunk_count, int big_chunk_count) {
        (void) main_key;
        (void) aux_key;
        (void) resume;
        (void) small_chunk_count;
        (void) big_chunk_count;

        // TODO implement this function!

        // max_small_block_size = small_chunk_size - static_cast<int>(sizeof(detail::mem_chunk));
        // other fields initialization here

        return NULL;
    }

    static size_t align_size(size_t size) {
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

    int __exchange_hash(size_ptr_hash *src, size_ptr_hash *dst, size_t k, shm_ptr v) {
        int ret = dst->insert(k, v);
        if (ret != 0) {
            ERR("cannot insert into hash map, key<%lu>, result<%d>.", k, ret);
            assert_noeffect(0);
            return ret;
        }

        ret = src->erase(k);
        if (ret != 0) {
            ERR("cannot erase from hash map, key<%lu>.", k);
            assert_noeffect(0);

            dst->erase(k);
            return ret;
        }

        return 0;
    }

    shm_ptr __malloc_from_free_hash(shm_ptr *chunk_ptr, void *&ptr) {
        mem_chunk *chunk = __ptr2chunk(*chunk_ptr);
        assert_retval(chunk, SHM_NULL);
        assert_retval(!chunk->full(), SHM_NULL);

        ptr = chunk->malloc();
        assert_retval(ptr, SHM_NULL);

        if (chunk->full()) {
            int ret = __exchange_hash(free_chunk_hash, used_chunk_hash,
                                      chunk->block_size, *chunk_ptr);
            // we do nothing here, if this happens, the next allocation of this memory
            // size will properly handle this
            if (ret != 0)
                assert_noeffect(0);
        }

        return ptr2ptr(ptr);
    }

    shm_ptr __malloc_from_free_store(bool use_small) {
        // TODO: implement this function
        (void) use_small;
        return SHM_NULL;
    }

    template<typename T>
    shm_ptr malloc(T *&t) {
        size_t mem_size = align_size(sizeof(T));
        bool use_small = mem_size <= max_small_block_size;

        shm_ptr *ptr = free_chunk_hash->find(mem_size);

        // if there is free chunk can hold this memory block
        if (ptr) {
            DBG("free chunk found, size<%ld>.", mem_size);

            mem_chunk *chunk = __ptr2chunk(*ptr);
            assert_retval(chunk, SHM_NULL);

            // this should never happen
            if (chunk->full()) {
                assert_noeffect(0);
                int ret = __exchange_hash(free_chunk_hash, used_chunk_hash, mem_size, *ptr);
                if (ret != 0)
                    return SHM_NULL;

                return malloc(t);
            }

            void *tmp = NULL;
            shm_ptr offset = __malloc_from_free_hash(ptr, tmp);
            assert_retval(offset != SHM_NULL, SHM_NULL);

            assert_noeffect(tmp);

            t = static_cast<T *>(tmp);
            return offset;
        }

        // now we need to allocate one block from free store
        DBG("no free chunk, allocate one, size<%ld>.", mem_size);



        // TODO add implementation here!

        return SHM_NULL;
    }
};

} // namespace sk

#endif // SHM_MGR_H
