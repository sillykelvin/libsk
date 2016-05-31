#ifndef FIXED_SET_H
#define FIXED_SET_H

#include "utility/utility.h"
#include "container/fixed_rbtree.h"

namespace sk {

template<typename T, size_t N>
struct fixed_set {
    typedef fixed_rbtree<T, T, identity<T>, N> impl_type;
    typedef typename impl_type::const_iterator iterator;
    typedef typename impl_type::const_iterator const_iterator;

    impl_type tree;

    bool empty() const { return tree.empty(); }

    bool full()  const { return tree.full(); }

    size_t size() const { return tree.size(); }

    size_t capacity() const { return tree.capacity(); }

    void clear() { tree.clear(); }

    int insert(const T& t) { return tree.insert(t); }

    void erase(iterator it) { tree.__erase(tree.__index(it.n)); }

    void erase(const T& t) { tree.erase(t); }

    iterator find(const T& t) { return tree.find(t); }

    const_iterator find(const T& t) const { return tree.find(t); }

    iterator begin() { return tree.begin(); }
    iterator end()   { return tree.end(); }
    const_iterator begin() const { return tree.begin(); }
    const_iterator end() const   { return tree.end(); }
};

} // namespace sk

#endif // FIXED_SET_H
