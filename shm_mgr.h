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

struct small_chunk {
    int magic;         // for memory overflow verification
    // TODO: seems free_count and free_head are duplicate, try to remove free_count
    int free_count;    // free block count
    int free_head;     // free head index
    size_t block_size;
    char data[0];

    /*
     * chunk_size: the size of entire memory chunk
     * block_size: the size of blocks in this chunk
     */
    int init(size_t chunk_size, size_t block_size) {
        magic = MAGIC;

        if (block_size % ALIGN_SIZE != 0)
            return -EINVAL;

        assert_retval(block_size >= sizeof(int), -1);

        free_count = (chunk_size - sizeof(small_chunk)) / block_size;
        free_head  = 0;
        this->block_size = block_size;

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

    int small_chunk_size;

    char *pool;
    shm_ptr singletons[ST_MAX];

    size_t lo_bound;
    size_t hi_bound;

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
        return static_cast<void *>(static_cast<char *>(static_cast<void *>(this)) + ptr);
    }

    shm_ptr ptr2ptr(void *ptr) {
        ptrdiff_t offset = static_cast<char *>(ptr) - static_cast<char *>(static_cast<void *>(this));
        assert_retval(offset >= 0, SHM_NULL);

        return offset;
    }

    int exchange_hash(size_ptr_hash *src, size_ptr_hash *dst, size_t k, shm_ptr v) {
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

    template<typename T>
    shm_ptr malloc(T *&t) {
        size_t mem_size = align_size(sizeof(T));
        bool use_small = (mem_size < small_chunk_size - sizeof(detail::small_chunk));

        shm_ptr *ptr = free_chunk_hash->find(mem_size);
        if (ptr) {
            DBG("free chunk found, size<%ld>.", mem_size);

            if (use_small) {
                detail::small_chunk *chunk = static_cast<detail::small_chunk *>(ptr2ptr(*ptr));
                assert_retval(chunk, SHM_NULL);

                // this should never happen
                if (chunk->full()) {
                    assert_noeffect(0);
                    int ret = exchange_hash(free_chunk_hash, used_chunk_hash, mem_size, *ptr);
                    if (ret != 0)
                        return SHM_NULL;

                    return malloc(t);
                }

                void *addr = chunk->malloc();
                assert_retval(addr, SHM_NULL);

                if (chunk->full()) {
                    int ret = exchange_hash(free_chunk_hash, used_chunk_hash, mem_size, *ptr);
                    // we do nothing here, if this happens, the next allocation of this memory
                    // size will properly handle this
                    if (ret != 0)
                        assert_noeffect(0);
                }

                return ptr2ptr(addr);
            } else {
                int ret = exchange_hash(free_chunk_hash, used_chunk_hash, mem_size, *ptr);
                if (ret != 0)
                    return SHM_NULL;

                return *ptr;
            }
        } else {
            DBG("no free chunk, alloc one, size<%ld>.", mem_size);
            // TODO add implementation here!
        }

        return SHM_NULL;
    }
};

} // namespace sk

#endif // SHM_MGR_H
