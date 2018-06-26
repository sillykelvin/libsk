#ifndef COROUTINE_H
#define COROUTINE_H

#include <uv.h>
#include <functional>
#include <utility/types.h>

NS_BEGIN(sk)

struct coroutine;
using coroutine_function = std::function<void()>;

void coroutine_init(uv_loop_t *loop);
void coroutine_fini();

coroutine *coroutine_create(const std::string& name,
                            const coroutine_function& fn,
                            size_t stack_size = 8 * 1024,
                            bool preserve_fpu = false,
                            bool protect_stack = false);

coroutine *coroutine_self();

const char *coroutine_name();

void coroutine_sleep(u64 ms);

void coroutine_yield();

void coroutine_schedule();

NS_END(sk)

#endif // COROUTINE_H
