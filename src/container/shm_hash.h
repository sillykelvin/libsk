#ifndef SHM_HASH_H
#define SHM_HASH_H

#include <iterator>
#include "log/log.h"
#include "shm/shm_ptr.h"
#include "utility/utility.h"

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
    typedef pair<K, V> value_type;

    value_type data;
    shm_ptr<shm_hash_node> next;

    shm_hash_node(const K& k, const V& v) : data(k, v), next() {}
};

/*
 * Note:
 *   1. T should be a shm_hash type here
 *   2. C stands for constness, true makes this iterator a const iterator
 */
template<typename T, bool C>
struct shm_hash_iterator {
    typedef T                                                     hash_type;
    typedef typename T::node                                      node_type;
    typedef typename T::value_type                                value_type;
    typedef ptrdiff_t                                             difference_type;
    typedef shm_hash_iterator<T, C>                               self;
    typedef std::bidirectional_iterator_tag                       iterator_category;
    typedef typename if_<C, const hash_type*, hash_type*>::type   hash_pointer;
    typedef typename if_<C, const node_type*, node_type*>::type   node_pointer;
    typedef typename if_<C, const value_type*, value_type*>::type pointer;
    typedef typename if_<C, const value_type&, value_type&>::type reference;

    hash_pointer h;
    node_pointer n;

    explicit shm_hash_iterator(hash_pointer h) : h(h), n(NULL) {}
    shm_hash_iterator(hash_pointer h, node_pointer n) : h(h), n(n) {}

    /*
     * make a templated copy constructor here to support construction of
     * const iterator from mutable iterator
     */
    template<bool B>
    shm_hash_iterator(const shm_hash_iterator<T, B>& s) : h(s.h), n(s.n) {}

    self& operator=(const self& s) {
        if (this == &s)
            return *this;

        h = s.h;
        n = s.n;

        return *this;
    }

    reference operator*() const { return n->data; }
    pointer operator->() const { return &n->data; }

    self& operator++() { n = h->__next(n); return *this; }
    self operator++(int) { self tmp(*this); n = h->__next(n); return tmp; }
    self& operator--()   { n = h->__prev(n); return *this; }
    self operator--(int) { self tmp(*this); n = h->__prev(n); return tmp; }

    bool operator==(const self& x) const { return n == x.n && h == x.h; }
    bool operator!=(const self& x) const { return !(*this == x); }

    bool operator==(const shm_hash_iterator<T, !C>& x) const { return n == x.n && h == x.h; }
    bool operator!=(const shm_hash_iterator<T, !C>& x) const { return !(*this == x); }
};

} // namespace detail

/**
 * K: hash key type
 * V: hash value type
 * F: the function to calculate hashcode for type K
 */
template<typename K, typename V, size_t(*F)(const K& k) = sk::detail::hashfunc>
struct shm_hash {
    typedef detail::shm_hash_node<K, V>            node;
    typedef typename node::value_type              value_type;
    typedef shm_ptr<node>                          pointer;
    typedef shm_hash<K, V, F>                      self;
    typedef detail::shm_hash_iterator<self, false> iterator;
    typedef detail::shm_hash_iterator<self, true>  const_iterator;

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
            sk_error("memory allocation failure, bucket_size<%lu>.", bucket_size);
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

    size_t size() const {
        return used_node_count;
    }

    size_t capacity() const {
        return total_node_count;
    }

    pointer __search(const K& k) {
        check_retval(!empty(), SHM_NULL);

        size_t hashcode = F(k);
        size_t idx = hashcode % bucket_size;

        pointer *base_addr = buckets.get();
        pointer p = base_addr[idx];
        while (p) {
            if (p->data.first == k)
                return p;

            p = p->next;
        }

        return SHM_NULL;
    }

    node *__next(node *n) {
        check_retval(n, NULL);

        // 1. if there is next node in current bucket, return it
        if (n->next)
            return n->next.get();

        size_t hashcode = F(n->data.first);
        size_t idx = hashcode % bucket_size;

        // 2. search next bucket, until one valid node is found or the end reached
        pointer *base_addr = buckets.get();
        while (true) {
            ++idx;

            check_break(idx < bucket_size);

            pointer p = base_addr[idx];
            if (p)
                return p.get();
        }

        return NULL;
    }

    const node *__next(const node *n) const {
        check_retval(n, NULL);

        if (n->next)
            return n->next.get();

        size_t hashcode = F(n->data.first);
        size_t idx = hashcode % bucket_size;

        pointer *base_addr = buckets.get();
        while (true) {
            ++idx;

            check_break(idx < bucket_size);

            pointer p = base_addr[idx];
            if (p)
                return p.get();
        }

        return NULL;
    }

    node *__prev(node *n) const {
        check_retval(n, NULL);

        size_t hashcode = F(n->data.first);
        size_t idx = hashcode % bucket_size;

        pointer *base_addr = buckets.get();
        pointer p = base_addr[idx];
        assert_retval(p, NULL);

        pointer prev = SHM_NULL;
        while (p) {
            if (p.get() == n)
                break;

            prev = p;
            p = p->next;
        }

        assert_retval(p && p.get() == n, NULL);

        if (prev) {
            assert_retval(prev->next == p, NULL);
            return prev.get();
        }

        assert_retval(base_addr[idx] == p, NULL);
        while (true) {
            if (idx <= 0)
                break;

            --idx;

            p = base_addr[idx];
            check_continue(p);

            while (p) {
                prev = p;
                p = p->next;
            }

            return prev.get();
        }

        return NULL;
    }

    const node *__prev(const node *n) const {
        check_retval(n, NULL);

        size_t hashcode = F(n->data.first);
        size_t idx = hashcode % bucket_size;

        pointer *base_addr = buckets.get();
        pointer p = base_addr[idx];
        assert_retval(p, NULL);

        pointer prev = SHM_NULL;
        while (p) {
            if (p.get() == n)
                break;

            prev = p;
            p = p->next;
        }

        assert_retval(p && p.get() == n, NULL);

        if (prev) {
            assert_retval(prev->next == p, NULL);
            return prev.get();
        }

        assert_retval(base_addr[idx] == p, NULL);
        while (true) {
            if (idx <= 0)
                break;

            --idx;

            p = base_addr[idx];
            check_continue(p);

            while (p) {
                prev = p;
                p = p->next;
            }

            return prev.get();
        }

        return NULL;
    }

    V *find(const K& k) {
        pointer p = __search(k);
        check_retval(p, NULL);

        return &p->data.second;
    }

    int insert(const K& k, const V& v) {
        check_retval(!full(), -ENOMEM);

        pointer p = __search(k);
        if (p) {
            p->data.second = v;
            return 0;
        }

        size_t hashcode = F(k);
        size_t idx = hashcode % bucket_size;

        p = shm_new<node>(k, v);
        if (!p) {
            sk_error("cannot allocate hash node.");
            return -ENOMEM;
        }

        pointer *base_addr = buckets.get();
        p->next = base_addr[idx];
        base_addr[idx] = p;

        ++used_node_count;

        return 0;
    }

    void erase(const K& k) {
        check_retnone(!empty());

        size_t hashcode = F(k);
        size_t idx = hashcode % bucket_size;

        bool found = false;
        pointer *base_addr = buckets.get();
        pointer prev = SHM_NULL;
        pointer p = base_addr[idx];
        while (p) {
            if (p->data.first == k) {
                found = true;
                break;
            }

            prev = p;
            p = p->next;
        }

        check_retnone(found);

        if (!prev) {
            sk_assert(base_addr[idx] == p);
            base_addr[idx] = p->next;
        } else {
            sk_assert(prev->next == p);
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

    iterator begin() {
        size_t idx = 0;

        pointer *base_addr = buckets.get();
        pointer p = SHM_NULL;
        while (!p) {
            check_break(idx < bucket_size);

            p = base_addr[idx++];
        }

        return iterator(this, p ? p.get() : NULL);
    }

    const_iterator begin() const {
        size_t idx = 0;

        pointer *base_addr = buckets.get();
        pointer p = SHM_NULL;
        while (!p) {
            check_break(idx < bucket_size);

            p = base_addr[idx++];
        }

        return const_iterator(this, p ? p.get() : NULL);
    }

    iterator end() { return iterator(this); }
    const_iterator end() const { return const_iterator(this); }
};

} // namespace sk

#endif // SHM_HASH_H
