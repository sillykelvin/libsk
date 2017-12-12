#ifndef FIXED_LIST_H
#define FIXED_LIST_H

#include <container/detail/list.h>
#include <container/detail/fixed_allocator.h>

NS_BEGIN(sk)

template<typename T>
using fixed_list_node = detail::list_node<T>;

template<typename T, size_t N>
using fixed_list_allocator = detail::fixed_allocator<sizeof(fixed_list_node<T>), N>;

template<typename T, size_t N>
using fixed_list = detail::list<T, fixed_list_allocator<T, N>>;

NS_END(sk)

#endif // FIXED_LIST_H
