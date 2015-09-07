#ifndef SHM_HASH_H
#define SHM_HASH_H

namespace sk {
namespace detail {

inline size_t hashfunc(const s32& key) {
    return static_cast<size_t>(key);
}

inline size_t hashfunc(const u32& key) {
    return static_cast<size_t>(key);
}

inline size_t hashfunc(const s64& key) {
    return static_cast<size_t>(key);
}

inline size_t hashfunc(const u64& key) {
    return static_cast<size_t>(key);
}

inline size_t hash_size(size_t max_node_count) {
    static const size_t sizes[] = {
        3, 13, 23, 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157,
        98317, 196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843
    };

    static const size_t nth_size = sizeof(sizes) / sizeof(size_t) - 1;
    for (size_t i = 0; i < nth_size; ++i) {
        if (max_node_count < sizes[i])
            return sizes[i];
    }

    return sizes[nth_size];
}

template<typename K, typename V>
struct shm_hash_node {
    K k;
    V v;
    shm_ptr<shm_hash_node> next;

    shm_hash_node(const K& k, const V& v) : k(k), v(v), next() {}
};

} // namespace detail

/**
 * K: hash key type
 * V: hash value type
 * F: the function to calculate hashcode for type K
 */
template<typename K, typename V, size_t(*F)(const K& k)>
struct shm_hash {
    typedef detail::shm_hash_node<K, V> node;
    typedef shm_ptr<node> pointer;

    size_t bucket_size;
    size_t used_node_count;
    size_t total_node_count;
    shm_ptr<pointer> buckets; // this is an array

    shm_hash(size_t max_node_count) {
        total_node_count = max_node_count;
        used_node_count = 0;
        bucket_size = detail::hash_size(max_node_count);

        size_t bucket_memory = bucket_size * sizeof(pointer);

        buckets = shm_malloc(bucket_memory);
        if (!buckets) {
            ERR("memory allocation failure, bucket_size<%lu>.", bucket_size);
            assert_retnone(0);
        }

        memset(buckets.get(), 0x00, bucket_memory);
    }

    ~shm_hash() {
        clear();
        shm_free(buckets);
    }

    bool full() const {
        return used_node_count >= total_node_count;
    }

    bool empty() const {
        return used_node_count <= 0;
    }

    pointer __search(const K& k) {
        if (empty())
            return SHM_NULL;

        size_t hashcode = F(k);
        size_t idx = hashcode % bucket_size;

        pointer *base_addr = buckets.get();
        pointer p = base_addr[idx];
        while (p) {
            if (p->k == k)
                return p;

            p = p->next;
        }

        return SHM_NULL;
    }

    V *find(const K& k) {
        pointer p = __search(k);
        if (!p)
            return NULL;

        return &p->v;
    }

    int insert(const K& k, const V& v) {
        if (full())
            return -ENOMEM;

        pointer p = __search(k);
        if (p) {
            p->v = v;
            return 0;
        }

        size_t hashcode = F(k);
        size_t idx = hashcode % bucket_size;

        p = shm_new<node>(k, v);
        if (!p) {
            ERR("cannot allocate hash node.");
            return -ENOMEM;
        }

        pointer *base_addr = buckets.get();
        p->next = base_addr[idx];
        base_addr[idx] = p;

        ++used_node_count;

        return 0;
    }

    void erase(const K& k) {
        if (empty())
            return;

        size_t hashcode = F(k);
        size_t idx = hashcode % bucket_size;

        bool found = false;
        pointer *base_addr = buckets.get();
        pointer prev = SHM_NULL;
        pointer p = base_addr[idx];
        while (p) {
            if (p->k == k) {
                found = true;
                break;
            }

            prev = p;
            p = p->next;
        }

        if (!found)
            return;

        if (!prev) {
            assert_noeffect(base_addr[idx] == p);
            base_addr[idx] = p->next;
        } else {
            assert_noeffect(prev->next == p);
            prev->next = p->next;
        }

        shm_del(p);
        --used_node_count;

        return;
    }

    void clear() {
        pointer *base_addr = buckets.get();
        for (size_t i = 0; i < bucket_size; ++i) {
            pointer p = base_addr[i];
            while (p) {
                pointer next = p->next;
                shm_del(p);
                p = next;
            }

            base_addr[i] = SHM_NULL;
        }

        used_node_count = 0;
    }
};

} // namespace sk

#endif // SHM_HASH_H
