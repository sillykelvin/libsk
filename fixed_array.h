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

    size_t elem_count;
    T elems[N];

    fixed_array() : elem_count(0) {}

    size_t size()     const { return elem_count; }
    size_t capacity() const { return N; }

    bool full()  const { return elem_count >= N; }
    bool empty() const { return elem_count <= 0; }

    T *emplace() {
        assert_retval(elem_count < N, NULL);
        return &elems[elem_count++];
    }

    T& operator[](size_t index) {
        assert_retval(index < elem_count, *((T*)(0)));

        return elems[index];
    }

    T *find(const T& value) {
        for (size_t i = 0; i < elem_count; ++i) {
            if (elems[i] == value)
                return &elems[i];
        }

        return NULL;
    }

    template<typename Pred>
    T *find(const T& value, Pred p) {
        for (size_t i = 0; i < elem_count; ++i) {
            if (p(elems[i], value))
                return &elems[i];
        }

        return NULL;
    }

    iterator begin() { return elems; }
    iterator end()   { return elems + elem_count; }
};

} // namespace sk

#endif // FIXED_ARRAY_H
