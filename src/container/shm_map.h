#ifndef SHM_MAP_H
#define SHM_MAP_H

#include "container/shm_rbtree.h"

namespace sk {

template<typename K, typename V>
struct shm_map {
    typedef pair<const K, V>                                 value_type;
    typedef shm_rbtree<K, value_type, select1st<value_type>> impl_type;
    typedef typename impl_type::iterator                     iterator;
    typedef typename impl_type::const_iterator               const_iterator;

    impl_type tree;

    bool empty() const { return tree.empty(); }

    size_t size() const { return tree.size(); }

    void clear() { tree.clear(); }

    int insert(const K& k, const V& v) { return tree.insert(value_type(k, v)); }

    void erase(iterator it) { tree.__erase(it.n); }

    void erase(const_iterator it) { tree.__erase(it.n); }

    void erase(const K& k) { tree.erase(k); }

    iterator find(const K& k) { return tree.find(k); }

    const_iterator find(const K& k) const { return tree.find(k); }

    iterator begin() { return tree.begin(); }
    iterator end()   { return tree.end(); }
    const_iterator begin() const { return tree.begin(); }
    const_iterator end() const   { return tree.end(); }
};

} // namespace sk

#endif // SHM_MAP_H
