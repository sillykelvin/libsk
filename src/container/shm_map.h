#ifndef SHM_MAP_H
#define SHM_MAP_H

#include <container/detail/rbtree.h>
#include <container/detail/shm_allocator.h>

NS_BEGIN(sk)

template<typename K, typename V>
using shm_map = detail::rbtree<K, pair<const K, V>,
                               false, std::less<K>,
                               detail::shm_allocator,
                               select1st<pair<const K, V>>>;

NS_END(sk)

#endif // SHM_MAP_H
