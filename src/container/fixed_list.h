#ifndef FIXED_LIST_H
#define FIXED_LIST_H

#include <container/detail/list.h>
#include <container/detail/fixed_allocator.h>

NS_BEGIN(sk)

template<typename T, size_t N>
using fixed_list_allocator = detail::fixed_allocator<bigger<sizeof(T), sizeof(shm_ptr<void>)>::value, N>;

template<typename T, size_t N>
using fixed_list = detail::list<T, fixed_list_allocator<T, N>>;

NS_END(sk)

#endif // FIXED_LIST_H
