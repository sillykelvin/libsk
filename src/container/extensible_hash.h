#ifndef EXTENSIBLE_HASH_H
#define EXTENSIBLE_HASH_H

namespace sk {
namespace detail {

template<typename K>
size_t hashcode(const K& k) {
    return static_cast<size_t>(k);
}

template<typename K, typename V>
struct extensible_hash_node {
    typedef pair<K, V> value_type;

    static const size_t npos = static_cast<size_t>(-1);

    char memory[sizeof(value_type)];
    size_t next;

    value_type *__pair() {
        return cast_ptr(value_type, memory);
    }

    void construct(const K& k, const V& v) {
        value_type *p = __pair();
        new (p) value_type(k, v);
    }

    void destroy() {
        value_type *p = __pair();
        p->~value_type();
    }

    const K& key() const   { return __pair()->first; }
    const V& value() const { return __pair()->second; }

    K& key()   { return __pair()->first; }
    V& value() { return __pair()->second; }
};

} // namespace detail

/**
 * K: hash key
 * V: hash value
 * D: allow duplication or not
 * F: the function to calculate hashcode for K
 */
template<typename K, typename V, bool D = true, size_t(*F)(const K& k) = detail::hashcode>
struct extensible_hash {
    typedef detail::extensible_hash_node<K, V> node;

    static const size_t npos = node::npos;

    size_t total_node_count;
    size_t used_node_count;
    size_t free_node_head;
    size_t bucket_size;
    size_t *buckets;
    node   *nodes;

    static size_t hash_size(size_t max_node_count) {
        static size_t sizes[] = {
            3, 13, 23, 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157,
            98317, 196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843
        };

        const size_t nth_size = sizeof(sizes) / sizeof(size_t) - 1;
        for (size_t i = 0; i < nth_size; ++i) {
            if (max_node_count < sizes[i])
                return sizes[i];
        }
        return sizes[nth_size];
    }

    static size_t calc_size(size_t max_node_count, size_t bucket_size = 0) {
        if (bucket_size == 0)
            bucket_size = hash_size(max_node_count);

        return sizeof(extensible_hash) + max_node_count * sizeof(node) + bucket_size * sizeof(size_t);
    }

    static extensible_hash *create(void *addr, size_t size, bool resume, size_t max_node_count, size_t bucket_size = 0) {
        assert_retval(addr, NULL);
        assert_retval(size >= calc_size(max_node_count, bucket_size), NULL);

        if (bucket_size == 0)
            bucket_size = hash_size(max_node_count);

        extensible_hash *self = cast_ptr(extensible_hash, addr);
        char *base_addr = char_ptr(addr);
        self->buckets = cast_ptr(size_t, base_addr + sizeof(extensible_hash));
        self->nodes = cast_ptr(node, base_addr + sizeof(extensible_hash) + sizeof(size_t) * bucket_size);

        if (resume) {
            assert_retval(self->total_node_count == max_node_count, NULL);
            assert_retval(self->used_node_count <= self->total_node_count, NULL);
            assert_retval(self->bucket_size == bucket_size, NULL);
        } else {
            self->total_node_count = max_node_count;
            self->used_node_count = 0;
            self->bucket_size = bucket_size;

            for (size_t i = 0; i < self->bucket_size; ++i)
                self->buckets[i] = npos;

            for (size_t i = 0; i < self->total_node_count; ++i)
                self->nodes[i].next = i + 1;

            self->nodes[self->total_node_count - 1].next = npos;

            self->free_node_head = 0;
        }

        return self;
    }

    bool empty() const { return used_node_count <= 0; }
    bool full() const  { return used_node_count >= total_node_count; }

    /**
     * @brief __search
     * @param k
     * @return the first node holds key k, NULL if not found
     *
     * NOTE: this function will always return the first matching node,
     *       even if there could be several nodes all hold key k.
     */
    node *__search(const K& k) {
        if (empty())
            return NULL;

        size_t hashcode = F(k);
        size_t bucket_idx = hashcode % bucket_size;

        size_t idx = buckets[bucket_idx];
        while (idx != npos) {
            node& n = nodes[idx];
            if (n.key() == k)
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
    V *find(const K& k) {
        node *n = __search(k);
        if (!n)
            return NULL;

        return &n->value();
    }

    int insert(const K& k, const V& v) {
        if (full())
            return -ENOMEM;

        /*
         * if duplication is not allowed
         */
        if (!D) {
            node *n = __search(k);
            if (n) {
                n->value() = v;
                return 0;
            }
        }

        size_t hashcode = F(k);
        size_t idx = hashcode % bucket_size;

        // if (buckets[idx] != IDX_NULL)
        //     DBG("hash collision detected, idx<%d>, hashcode<%ld>.", idx, hashcode);

        size_t node_idx = free_node_head;
        node& n = nodes[node_idx];

        free_node_head = n.next;

        n.construct(k, v);
        n.next = buckets[idx];
        buckets[idx] = node_idx;

        ++used_node_count;

        return 0;
    }

    /**
     * @brief erase
     * @param k
     *
     * NOTE: like search, it will always operate on the first found node
     */
    void erase(const K& k) {
        if (empty())
            return;

        size_t hashcode = F(k);
        size_t bucket_idx = hashcode % bucket_size;

        bool found = false;
        size_t prev = npos;
        size_t idx = buckets[bucket_idx];
        while (idx != npos) {
            node& n = nodes[idx];
            if (n.key() == k){
                found = true;
                break;
            }

            prev = idx;
            idx = n.next;
        }

        if (!found)
            return;

        if (prev == npos) {
            sk_assert(buckets[bucket_idx] == idx);
            buckets[bucket_idx] = nodes[idx].next;
        } else {
            sk_assert(nodes[prev].next == idx);
            nodes[prev].next = nodes[idx].next;
        }

        nodes[idx].destroy();
        nodes[idx].next = free_node_head;
        free_node_head = idx;
        --used_node_count;

        return;
    }

    /**
     * @brief erase
     * @param k
     * @param v
     *
     * NOTE: this function will erase the node whose key and value
     *       both exactly match the given parameters
     *
     * TODO: this function has much same code with erase(const K& k),
     *       consider to refactor it
     */
    void erase(const K& k, const V& v) {
        if (!D)
            return erase(k);

        size_t hashcode = F(k);
        size_t bucket_idx = hashcode % bucket_size;

        bool found = false;
        size_t prev = npos;
        size_t idx = buckets[bucket_idx];
        while (idx != npos) {
            node& n = nodes[idx];
            if (n.key() == k && n.value() == v) {
                found = true;
                break;
            }

            prev = idx;
            idx = n.next;
        }

        if (!found)
            return;

        if (prev == npos) {
            sk_assert(buckets[bucket_idx] == idx);
            buckets[bucket_idx] = nodes[idx].next;
        } else {
            sk_assert(nodes[prev].next == idx);
            nodes[prev].next = nodes[idx].next;
        }

        nodes[idx].destroy();
        nodes[idx].next = free_node_head;
        free_node_head = idx;
        --used_node_count;

        return;
    }
};

} // namespace sk

#endif // EXTENSIBLE_HASH_H
