#ifndef FIXED_STACK_H
#define FIXED_STACK_H

#include <utility>
#include "utility/types.h"
#include "container/fixed_vector.h"

namespace sk {

template<typename T, size_t N>
struct fixed_stack {
    typedef fixed_vector<T, N> impl_type;
    typedef typename impl_type::iterator iterator;
    typedef typename impl_type::const_iterator const_iterator;

    impl_type stack;

    size_t size()     const { return stack.size(); }
    size_t capacity() const { return stack.capacity(); }

    bool full() const { return stack.full(); }
    bool empty() const { return stack.empty(); }

    template<typename... Args>
    T *emplace(Args&&... args) {
        return stack.emplace(std::forward<Args>(args)...);
    }

    T *top() {
        if (empty())
            return NULL;

        return stack.end() - 1;
    }

    void pop() {
        if (empty())
            return;

        stack.erase(stack.end() - 1);
    }

    iterator begin() { return stack.begin(); }
    iterator end()   { return stack.end(); }
    const_iterator begin() const { return stack.begin(); }
    const_iterator end()   const { return stack.end(); }
};

} // namespace sk

#endif // FIXED_STACK_H
