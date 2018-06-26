#include <log/log.h>
#include <coroutine/coroutine.h>
#include <coroutine/detail/coroutine_mgr.h>

NS_BEGIN(sk)

static detail::coroutine_mgr *mgr = nullptr;

void coroutine_init(uv_loop_t *loop) {
    if (!mgr) {
        mgr = new detail::coroutine_mgr(loop);
    }
}

void coroutine_fini() {
    if (mgr) {
        delete mgr;
        mgr = nullptr;
    }
}

coroutine *coroutine_create(const std::string& name,
                            const coroutine_function& fn,
                            size_t stack_size, bool preserve_fpu) {
    return mgr->create(name, fn, stack_size, preserve_fpu);
}

coroutine *coroutine_self() {
    return mgr->self();
}

const char *coroutine_name() {
    return mgr->name();
}

void coroutine_sleep(u64 ms) {
    if (!coroutine_self()) {
        sk_error("coroutine_sleep() can only be called inside a coroutine.");
        return;
    }

    return mgr->sleep(ms);
}

void coroutine_yield() {
    if (!coroutine_self()) {
        sk_error("coroutine_yield() can only be called inside a coroutine.");
        return;
    }

    return mgr->yield();
}

void coroutine_schedule() {
    if (coroutine_self()) {
        sk_error("coroutine_schedule() cannot be called inside a coroutine.");
        return;
    }

    return mgr->schedule();
}

NS_END(sk)
