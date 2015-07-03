#ifndef STACK_H
#define STACK_H

namespace sk {

template<typename T, size_t N>
struct fixed_stack {
    typedef fixed_array<T, N> impl_type;
    typedef typename impl_type::iterator iterator;

    impl_type nodes;

    size_t size()     const { return nodes.size(); }
    size_t capacity() const { return nodes.capacity(); }

    bool full() const { return nodes.full(); }
    bool empty() const { return nodes.empty(); }

    T *emplace() {
        return nodes.emplace();
    }

    T *top() {
        if (empty())
            return NULL;

        return nodes.end() - 1;
    }

    void pop() {
        if (empty())
            return;

        nodes.erase(nodes.end() - 1);
    }

    iterator begin() { return nodes.begin(); }
    iterator end()   { return nodes.end(); }
};

} // namespace sk

#endif // STACK_H
