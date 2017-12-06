#ifndef SHM_LIST_H
#define SHM_LIST_H

#include <container/detail/list.h>
#include <container/detail/shm_allocator.h>

NS_BEGIN(sk)

template<typename T>
using shm_list = detail::list<T, detail::shm_allocator>;

NS_END(sk)

#endif // SHM_LIST_H
