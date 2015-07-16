#ifndef FIXED_MAP_H
#define FIXED_MAP_H

namespace sk {

template<typename K, typename V, size_t N>
struct fixed_map {
    typedef pair<const K, V>                                      value_type;
    typedef fixed_rbtree<K, value_type, select1st<value_type>, N> impl_type;
    typedef typename impl_type::iterator                          iterator;
    typedef typename impl_type::const_iterator                    const_iterator;

    impl_type tree;

    bool empty() const { return tree.empty(); }

    bool full()  const { return tree.full(); }

    size_t size() const { return tree.size(); }

    size_t capacity() const { return tree.capacity(); }

    void clear() { tree.clear(); }

    int insert(const K& k, const V& v) { return tree.insert(value_type(k, v)); }

    void erase(iterator it) { tree.__erase(tree.__index(it.n)); }

    void erase(const_iterator it) { tree.__erase(tree.__index(it.n)); }

    void erase(const K& k) { tree.erase(k); }

    iterator find(const K& k) { return tree.find(k); }

    const_iterator find(const K& k) const { return tree.find(k); }

    iterator begin() { return tree.begin(); }
    iterator end()   { return tree.end(); }
    const_iterator begin() const { return tree.begin(); }
    const_iterator end() const   { return tree.end(); }
};

} // namespace sk

#endif // FIXED_MAP_H
