#ifndef FIXED_VECTOR_H
#define FIXED_VECTOR_H

namespace sk {

/*
 * fixed_vector is a vector with fixed size, can be used
 * in both normal memory and shared memory.
 *
 * TODO: implement a specialization version for bool
 */
template<typename T, size_t N>
struct fixed_vector {
    typedef T* iterator;
    typedef const T* const_iterator;

    size_t count;
    char memory[sizeof(T) * N];

    fixed_vector() : count(0) {}
    ~fixed_vector() { clear(); }

    fixed_vector(const fixed_vector& vector) : count(0) {
        fixed_vector::const_iterator it, end;
        for (it = vector.begin(), end = vector.end(); it != end; ++it)
            emplace(*it);
    }

    fixed_vector& operator=(const fixed_vector& vector) {
        if (this == &vector)
            return *this;

        clear();

        fixed_vector::const_iterator it, end;
        for (it = vector.begin(), end = vector.end(); it != end; ++it)
            emplace(*it);

        return *this;
    }

    size_t size()     const { return count; }
    size_t capacity() const { return N; }

    bool full()  const { return count >= N; }
    bool empty() const { return count <= 0; }

    void clear() {
        iterator it, end;
        for (it = this->begin(), end = this->end(); it != end; ++it)
            it->~T();

        count = 0;
    }

    void fill(size_t n, const T& value) {
        assert_retnone(n <= capacity());

        if (n >= size()) {
            std::fill(begin(), end(), value);
            size_t left = n - size();
            while (left-- > 0)
                emplace(value);

            return;
        }

        std::fill(begin(), at(n), value);
        for (size_t i = count - 1; i >= n; --i)
            erase_at(i);
    }

    iterator at(size_t index) {
        assert_retval(index < count, NULL);

        return begin() + index;
    }

    void erase_at(size_t index) {
        assert_retnone(index < count);

        if (at(index) + 1 != end())
            std::copy(at(index) + 1, end(), at(index));

        at(count - 1)->~T();
        --count;
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

    template<typename... Args>
    T* emplace(Args&&... args) {
        if (full())
            return NULL;

        T *t = at(count++);
        new (t) T(std::forward<Args>(args)...);

        return t;
    }

    T& operator[](size_t index) {
        return *at(index);
    }

    iterator find(const T& value) {
        iterator it, end;
        for (it = this->begin(), end = this->end(); it != end; ++it) {
            if (*it == value)
                return it;
        }

        return NULL;
    }

    template<typename Pred>
    iterator find_if(Pred p) {
        iterator it, end;
        for (it = this->begin(), end = this->end(); it != end; ++it) {
            if (p(*it))
                return it;
        }

        return NULL;
    }

    iterator begin() { return cast_ptr(T, memory); }
    iterator end()   { return cast_ptr(T, memory) + count; }
    const_iterator begin() const { return reinterpret_cast<const T*>(memory); }
    const_iterator end()   const { return reinterpret_cast<const T*>(memory) + count; }
};

} // namespace sk

#endif // FIXED_VECTOR_H
