#ifndef CONTEXT_H
#define CONTEXT_H

#include <utility/types.h>
#include <coroutine/coroutine.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

class context {
public:
    MAKE_NONCOPYABLE(context);

    static context *create(const coroutine_function& fn,
                           size_t stack_size, bool preserve_fpu = false);
    ~context();

    bool swap_in();
    bool swap_out();

private:
    context() : ctx_(nullptr), stack_(nullptr), stack_size_(0), preserve_fpu_(false) {}

    static void context_main(intptr_t arg) {
        context *ctx = reinterpret_cast<context*>(arg);
        ctx->fn_();
    }

private:
    void *ctx_;
    char *stack_;
    size_t stack_size_;
    bool preserve_fpu_;
    coroutine_function fn_;
};

NS_END(detail)
NS_END(sk)

#endif // CONTEXT_H
