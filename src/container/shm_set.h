#ifndef SHM_SET_H
#define SHM_SET_H

#include "container/shm_rbtree.h"

namespace sk {

template<typename T>
struct shm_set {
    typedef shm_rbtree<T, T, identity<T>>      impl_type;
    typedef typename impl_type::const_iterator iterator;
    typedef typename impl_type::const_iterator const_iterator;

    impl_type tree;

    bool empty() const { return tree.empty(); }

    size_t size() const { return tree.size(); }

    void clear() { tree.clear(); }

    int insert(const T& t) { return tree.insert(t); }

    void erase(iterator it) { tree.__erase(it.n); }

    void erase(const T& t) { tree.erase(t); }

    iterator find(const T& t) { return tree.find(t); }

    const_iterator find(const T& t) const { return tree.find(t); }

    iterator begin() { return tree.begin(); }
    iterator end()   { return tree.end(); }
    const_iterator begin() const { return tree.begin(); }
    const_iterator end() const   { return tree.end(); }
};

} // namespace sk

#endif // SHM_SET_H
