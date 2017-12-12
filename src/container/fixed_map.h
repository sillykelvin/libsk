#ifndef FIXED_MAP_H
#define FIXED_MAP_H

#include <container/detail/rbtree.h>
#include <container/detail/fixed_allocator.h>

NS_BEGIN(sk)

template<typename K, typename V>
using fixed_map_pair = pair<const K, V>;

template<typename K, typename V>
using fixed_map_node = detail::rbtree_node<fixed_map_pair<K, V>>;

template<typename K, typename V, size_t N>
using fixed_map_allocator = detail::fixed_allocator<sizeof(fixed_map_node<K, V>), N>;

template<typename K, typename V, size_t N>
using fixed_map = detail::rbtree<K, fixed_map_pair<K, V>,
                                 false, std::less<K>,
                                 fixed_map_allocator<K, V, N>,
                                 select1st<fixed_map_pair<K, V>>>;

NS_END(sk)

#endif // FIXED_MAP_H
