#ifndef EXTENSIBLE_STACK_H
#define EXTENSIBLE_STACK_H

namespace sk {

template<typename T>
struct extensible_stack {
    size_t used_count;
    size_t total_count;
    char data[0];

    static size_t calc_size(size_t max_count) {
        return sizeof(extensible_stack) + sizeof(T) * max_count;
    }

    static extensible_stack *create(void *addr, size_t size, size_t max_count, bool resume) {
        assert_retval(addr, NULL);
        assert_retval(size >= calc_size(max_count), NULL);

        extensible_stack *self = cast_ptr(extensible_stack, addr);

        if (resume)
            assert_noeffect(self->total_count == max_count);
        else {
            self->used_count = 0;
            self->total_count = max_count;
        }

        return self;
    }

    bool empty() const { return used_count <= 0; }
    bool full() const  { return used_count >= total_count; }

    template<typename... Args>
    int push(Args&&... args) {
        if (full())
            return -ENOMEM;

        T *t = cast_ptr(T, data) + used_count;
        new (t) T(std::forward<Args>(args)...);
        ++used_count;

        return 0;
    }

    T *top() {
        if (empty())
            return NULL;

        return cast_ptr(T, data) + (used_count - 1);
    }

    void pop() {
        T *t = top();
        if (!t)
            return;

        t->~T();
        --used_count;
    }
};

} // namespace sk

#endif // EXTENSIBLE_STACK_H
