#ifndef FIXED_SET_H
#define FIXED_SET_H

#include <container/detail/rbtree.h>
#include <container/detail/fixed_allocator.h>

NS_BEGIN(sk)

template<typename T>
using fixed_set_node = detail::rbtree_node<T>;

template<typename T, size_t N>
using fixed_set_allocator = detail::fixed_allocator<sizeof(fixed_set_node<T>), N>;

template<typename T, size_t N>
using fixed_set = detail::rbtree<T, T, true, std::less<T>,
                                 fixed_set_allocator<T, N>, identity<T>>;

NS_END(sk)

#endif // FIXED_SET_H
