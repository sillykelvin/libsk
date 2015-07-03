#ifndef FIXED_ARRAY_H
#define FIXED_ARRAY_H

namespace sk {

/*
 * fixed_array is an array with fixed size, can be used
 * in both normal memory and shared memory.
 *
 * TODO: implement a specialization version for bool
 */
template<typename T, size_t N>
struct fixed_array {
    typedef T* iterator;
    typedef const T* const_iterator;

    size_t elem_count;
    /*
     * do NOT use T elems[N] here, this will call constructor of T,
     * however, we should call constructor when we add new instance
     */
    char elems[sizeof(T) * N];

    fixed_array() : elem_count(0) {}
    ~fixed_array() { clear(); }

    fixed_array(const fixed_array& array) : elem_count(0) {
        if (this == &array)
            return;

        fill(array.size(), T());
        std::copy(array.begin(), array.end(), begin());
    }

    fixed_array& operator=(const fixed_array& array) {
        if (this == &array)
            return *this;

        fill(array.size(), T());
        std::copy(array.begin(), array.end(), begin());

        return *this;
    }

    size_t size()     const { return elem_count; }
    size_t capacity() const { return N; }

    bool full()  const { return elem_count >= N; }
    bool empty() const { return elem_count <= 0; }

    void clear() {
        for (iterator it = begin(); it != end(); ++it)
            it->~T();

        elem_count = 0;
    }

    void fill(size_t n, const T& value) {
        assert_retnone(n <= capacity());

        if (n >= size()) {
            std::fill(begin(), end(), value);
            size_t left = n - size();
            while (left-- > 0) {
                T *t = emplace();
                *t = value;
            }

            return;
        }

        std::fill(begin(), at(n), value);
        for (size_t i = elem_count - 1; i >= n; --i)
            erase_at(i);
    }

    iterator at(size_t index) {
        assert_retval(index < elem_count, NULL);

        return begin() + index;
    }

    void erase_at(size_t index) {
        assert_retnone(index < elem_count);

        if (at(index) + 1 != end())
            std::copy(at(index) + 1, end(), at(index));

        at(elem_count - 1)->~T();
        --elem_count;
    }

    void erase(iterator it) {
        assert_retnone(it);

        size_t index = it - begin();
        erase_at(index);
    }

    void erase(const T& value) {
        iterator it = find(value);
        if (!it)
            return;

        erase(it);
    }

    template<typename Pred>
    void erase_if(Pred p) {
        iterator it = find_if(p);
        if (!it)
            return;

        erase(it);
    }

    T *emplace() {
        if (full())
            return NULL;

        T *t = at(elem_count++);
        new (t) T();

        return t;
    }

    T& operator[](size_t index) {
        return *at(index);
    }

    iterator find(const T& value) {
        for (iterator it = begin(); it != end(); ++it) {
            if (*it == value)
                return it;
        }

        return NULL;
    }

    template<typename Pred>
    iterator find_if(Pred p) {
        for (iterator it = begin(); it != end(); ++it) {
            if (p(*it))
                return it;
        }

        return NULL;
    }

    iterator begin() { return cast_ptr(T, elems); }
    iterator end()   { return cast_ptr(T, elems) + elem_count; }
    const_iterator begin() const { return reinterpret_cast<const T*>(elems); }
    const_iterator end()   const { return reinterpret_cast<const T*>(elems) + elem_count; }
};

} // namespace sk

#endif // FIXED_ARRAY_H
