#ifndef REFERABLE_ARRAY_H
#define REFERABLE_ARRAY_H

namespace sk {

namespace detail {

template<typename T>
struct array_node {
    bool used;
    char data[sizeof(T)];

    // make array_node a POD type by disable constructor, otherwise, GCC will complaint:
    //     error: (perhaps the 'offsetof' macro was used incorrectly)
    // in function index(...)
    // array_node(size_t next) : used(false) { *cast_ptr(size_t, data) = next; }
};

} // namespace detail

template<typename T, size_t N>
struct referable_array {
    static_assert(sizeof(T) >= sizeof(size_t), "type size not enough");

    typedef detail::array_node<T> node;
    static const size_t npos = static_cast<size_t>(-1);

    size_t used_count;
    size_t free_head;
    char memory[sizeof(node) * N];

    referable_array() : used_count(0), free_head(0) { __init(); }
    ~referable_array() { clear(); }

    /*
     * Note: we cannot just put the nodes in array into this container, as this
     * is a referable container, the data type T may contains index reference,
     * so we MUST keep the index unchanged. so does the assignment operator
     */
    referable_array(const referable_array& array) { __copy(array); }

    referable_array& operator=(const referable_array& array) {
        if (this == &array)
            return *this;

        clear();
        __copy(array);
        return *this;
    }

    void __init() {
        // link the free slots
        for (size_t i = 0; i < N; ++i) {
            node *n = __at(i);
            new (n) node();
            n->used = false;
            *cast_ptr(size_t, n->data) = (i == N - 1) ? npos : i + 1;
        }
    }

    void __copy(const referable_array& array) {
        for (size_t i = 0; i < array.capacity(); ++i) {
            const node *on = array.__at(i); // old node
            node *nn = __at(i);             // new node

            if (on->used) {
                nn->used = true;
                const T* ot = cast_ptr(T, const_cast<char*>(on->data)); // old T
                T* nt = cast_ptr(T, nn->data);                          // new T
                new (nt) T(*ot);
            } else {
                nn->used = false;
                const size_t* oi = cast_ptr(size_t, const_cast<char*>(on->data)); // old index
                size_t* ni = cast_ptr(size_t, nn->data);                          // new index
                *ni = *oi;
            }
        }

        used_count = array.used_count;
        free_head = array.free_head;
    }

    node *__at(size_t index) {
        return cast_ptr(node, memory) + index;
    }

    const node *__at(size_t index) const {
        return cast_ptr(node, const_cast<char *>(memory)) + index;
    }

    /*
     * Note: this function does NOT check whether the element's
     * destructor has been called, the caller should do the job
     */
    void __add_free(size_t index) {
        node *n = __at(index);
        n->used = false;
        *cast_ptr(size_t, n->data) = free_head;

        free_head = index;
        --used_count;
    }

    /*
     * Note: this function does NOT check whether the element's
     * constructor has been called, the caller should do the job
     */
    void __add_used(size_t index) {
        __at(index)->used = true;
        ++used_count;
    }

    size_t size()     const { return used_count; }
    size_t capacity() const { return N; }

    bool full()  const {
        if (used_count >= N) {
            assert_noeffect(free_head == npos);
            return true;
        }
        return false;
    }

    bool empty() const {
        if (used_count <= 0) {
            assert_retval(free_head != npos, false);
            return true;
        }
        return false;
    }

    void clear() {
        for (size_t i = 0; i < N; ++i) {
            node *n = __at(i);
            if (!n->used)
                continue;

            T *t = cast_ptr(T, n->data);
            t->~T();
            __add_free(i);
        }
    }

    T *at(size_t index) {
        assert_retval(index < N, NULL);

        node *n = __at(index);
        if (n->used)
            return cast_ptr(T, n->data);

        return NULL;
    }

    const T *at(size_t index) const {
        assert_retval(index < N, NULL);

        const node *n = __at(index);
        if (n->used)
            return cast_ptr(T, const_cast<char*>(n->data));

        return NULL;
    }

    void erase(size_t index) {
        assert_retnone(index < N);

        node *n = __at(index);
        if (!n->used)
            return;

        T *t = cast_ptr(T, n->data);
        t->~T();
        __add_free(index);
    }

    template<typename... Args>
    T *emplace(size_t *index, Args&&... args) {
        if (full())
            return NULL;

        assert_retval(free_head != npos, NULL);

        node *n = __at(free_head);
        if (index)
            *index = free_head;
        __add_used(free_head);
        free_head = *cast_ptr(size_t, n->data);

        T *t = cast_ptr(T, n->data);
        new (t) T(std::forward<Args>(args)...);

        return t;
    }

    size_t index(const T *t) const {
        assert_retval(t, npos);

        node *n = cast_ptr(node, char_ptr(const_cast<T*>(t)) - offsetof(node, data));
        assert_noeffect(n->used);

        size_t offset = char_ptr(n) - memory;
        assert_noeffect(offset % sizeof(node) == 0);

        return offset / sizeof(node);
    }
};

} // namespace sk

#endif // REFERABLE_ARRAY_H
