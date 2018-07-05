#include <log/log.h>
#include <coroutine/coroutine.h>
#include <coroutine/detail/coroutine_mgr.h>

#define ensure_coroutine(handle, type)                                              \
    do {                                                                            \
        int t = coroutine_handle_type(handle);                                      \
        if (t != type) {                                                            \
            sk_error("handle type mismatch, this: %d, wanted: %d.", t, type);       \
            return -EINVAL;                                                         \
        }                                                                           \
                                                                                    \
        coroutine *self = coroutine_self();                                         \
        if (!self || coroutine_get(handle) != self) {                               \
            sk_error("this function can only be called inside its own coroutine."); \
            return -EINVAL;                                                         \
        }                                                                           \
    } while (0)

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

coroutine *coroutine_create(const std::string& name, const coroutine_function& fn,
                            size_t stack_size, bool preserve_fpu, bool protect_stack) {
    return mgr->create(name, fn, stack_size, preserve_fpu, protect_stack);
}

coroutine *coroutine_self() {
    return mgr->self();
}

const char *coroutine_name() {
    return mgr->name();
}

void coroutine_sleep(u64 ms) {
    if (!coroutine_self()) {
        sk_error("this function can only be called inside a coroutine.");
        return;
    }

    return mgr->sleep(ms);
}

void coroutine_yield() {
    if (!coroutine_self()) {
        sk_error("this function can only be called inside a coroutine.");
        return;
    }

    return mgr->yield();
}

void coroutine_schedule() {
    if (coroutine_self()) {
        sk_error("this function cannot be called inside a coroutine.");
        return;
    }

    return mgr->schedule();
}

coroutine *coroutine_get(coroutine_handle *h) {
    return mgr->get(h);
}

void coroutine_bind(coroutine_handle *h, coroutine *c) {
    return mgr->bind(h, c);
}

coroutine_handle *coroutine_handle_create(int type, coroutine *c) {
    if (!c) {
        c = coroutine_self();
        if (!c) {
            sk_error("cannot bind handle to a coroutine.");
            return nullptr;
        }
    }

    return mgr->handle_create(type, c);
}

void coroutine_handle_close(coroutine_handle *h) {
    coroutine *self = coroutine_self();
    if (!self || coroutine_get(h) != self) {
        sk_error("this function can only be called inside its own coroutine.");
        return;
    }

    return mgr->handle_close(h);
}

void coroutine_handle_free(coroutine_handle *h) {
    coroutine *self = coroutine_self();
    if (!self || coroutine_get(h) != self) {
        sk_error("this function can only be called inside its own coroutine.");
        return;
    }

    return mgr->handle_free(h);
}

int coroutine_handle_type(coroutine_handle *h) {
    return mgr->handle_type(h);
}

int coroutine_tcp_bind(coroutine_handle *h, u16 port) {
    ensure_coroutine(h, coroutine_handle_tcp);
    return mgr->tcp_bind(h, port);
}

int coroutine_tcp_connect(coroutine_handle *h, const std::string& host, u16 port) {
    ensure_coroutine(h, coroutine_handle_tcp);
    return mgr->tcp_connect(h, host, port);
}

int coroutine_tcp_listen(coroutine_handle *h, int backlog) {
    ensure_coroutine(h, coroutine_handle_tcp);
    return mgr->tcp_listen(h, backlog);
}

int coroutine_tcp_accept(coroutine_handle *h, coroutine_handle *client) {
    ensure_coroutine(h, coroutine_handle_tcp);
    return mgr->tcp_accept(h, client);
}

int coroutine_tcp_shutdown(coroutine_handle *h) {
    ensure_coroutine(h, coroutine_handle_tcp);
    return mgr->tcp_shutdown(h);
}

ssize_t coroutine_tcp_read(coroutine_handle *h, void *buf, size_t len) {
    ensure_coroutine(h, coroutine_handle_tcp);
    return mgr->tcp_read(h, buf, len);
}

ssize_t coroutine_tcp_write(coroutine_handle *h, const void *buf, size_t len) {
    ensure_coroutine(h, coroutine_handle_tcp);
    return mgr->tcp_write(h, buf, len);
}

int coroutine_fs_add_watch(coroutine_handle *h, const std::string& file) {
    ensure_coroutine(h, coroutine_handle_fs_watcher);
    return mgr->fs_add_watch(h, file);
}

int coroutine_fs_rm_watch(coroutine_handle *h, const std::string& file) {
    ensure_coroutine(h, coroutine_handle_fs_watcher);
    return mgr->fs_rm_watch(h, file);
}

int coroutine_fs_watch(coroutine_handle *h, std::vector<coroutine_fs_event> *events) {
    ensure_coroutine(h, coroutine_handle_fs_watcher);
    return mgr->fs_watch(h, events);
}

NS_END(sk)
