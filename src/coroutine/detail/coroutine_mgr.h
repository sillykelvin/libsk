#ifndef COROUTINE_MGR_H
#define COROUTINE_MGR_H

#include <uv.h>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <time/heap_timer.h>
#include <coroutine/coroutine.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

class coroutine_mgr {
public:
    explicit coroutine_mgr(uv_loop_t *loop);
    ~coroutine_mgr();

    coroutine *create(const std::string& name, const coroutine_function& fn,
                      size_t stack_size, bool preserve_fpu, bool protect_stack);

    uv_loop_t *loop();

    coroutine *self();
    const char *name();

    void sleep(u64 ms);
    void yield();
    void schedule();

    static void context_main(intptr_t arg);

private:
    static void yield(coroutine *c);
    static void resume(coroutine *c);

    static void on_sleep_timeout(coroutine *c, heap_timer *t);

private:
    uv_loop_t *loop_;
    coroutine *uv_;
    coroutine *current_;
    std::queue<coroutine*> runnable_;
    std::unordered_set<coroutine*> io_waiting_;
    std::unordered_set<coroutine*> cond_waiting_;
};

NS_END(detail)
NS_END(sk)

#endif // COROUTINE_MGR_H
