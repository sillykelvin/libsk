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

    referable_array() : used_count(0), free_head(0) {
        // link the free slots
        for (size_t i = 0; i < N; ++i) {
            node *n = __at(i);
            new (n) node();
            n->used = false;
            *cast_ptr(size_t, n->data) = (i == N - 1) ? npos : i + 1;
            // new (n) node((i == N - 1) ? npos : i + 1);
        }
    }

    ~referable_array() { clear(); }

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

    T *emplace(size_t *index = NULL) {
        if (full())
            return NULL;

        assert_retval(free_head != npos, NULL);

        node *n = __at(free_head);
        if (index)
            *index = free_head;
        __add_used(free_head);
        free_head = *cast_ptr(size_t, n->data);

        T *t = cast_ptr(T, n->data);
        new (t) T();

        return t;
    }

    size_t index(T *t) const {
        assert_retval(t, npos);

        node *n = cast_ptr(node, char_ptr(t) - offsetof(node, data));
        assert_noeffect(n->used);

        size_t offset = char_ptr(n) - memory;
        assert_noeffect(offset % sizeof(node) == 0);

        return offset / sizeof(node);
    }
};

} // namespace sk

#endif // REFERABLE_ARRAY_H
