#ifndef SHM_SET_H
#define SHM_SET_H

#include <container/detail/rbtree.h>
#include <container/detail/shm_allocator.h>

NS_BEGIN(sk)

template<typename T>
using shm_set = detail::rbtree<T, T, true, std::less<T>,
                               detail::shm_allocator, identity<T>>;

NS_END(sk)

#endif // SHM_SET_H
