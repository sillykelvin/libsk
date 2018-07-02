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

coroutine *coroutine_get(coroutine_tcp_handle *h) {
    return mgr->get(h);
}

void coroutine_bind(coroutine_tcp_handle *h, coroutine *c) {
    return mgr->bind(h, c);
}

coroutine_tcp_handle *coroutine_tcp_create(coroutine *c) {
    // bind to current coroutine if coroutine is not specified
    if (!c) {
        c = coroutine_self();
        if (!c) {
            sk_error("cannot bind handle to a coroutine.");
            return nullptr;
        }
    }

    return mgr->tcp_create(c);
}

void coroutine_tcp_free(coroutine_tcp_handle *h) {
    coroutine *self = coroutine_self();
    if (!self || coroutine_get(h) != self) {
        sk_error("coroutine_tcp_free() can only be called inside its own coroutine.");
        return;
    }

    return mgr->tcp_free(h);
}

void coroutine_tcp_close(coroutine_tcp_handle *h) {
    coroutine *self = coroutine_self();
    if (!self || coroutine_get(h) != self) {
        sk_error("coroutine_tcp_close() can only be called inside its own coroutine.");
        return;
    }

    return mgr->tcp_close(h);
}

int coroutine_tcp_bind(coroutine_tcp_handle *h, u16 port) {
    coroutine *self = coroutine_self();
    if (!self || coroutine_get(h) != self) {
        sk_error("coroutine_tcp_bind() can only be called inside its own coroutine.");
        return -ESRCH;
    }

    return mgr->tcp_bind(h, port);
}

int coroutine_tcp_connect(coroutine_tcp_handle *h, const std::string& host, u16 port) {
    coroutine *self = coroutine_self();
    if (!self || coroutine_get(h) != self) {
        sk_error("coroutine_tcp_connect() can only be called inside its own coroutine.");
        return -ESRCH;
    }

    return mgr->tcp_connect(h, host, port);
}

int coroutine_tcp_listen(coroutine_tcp_handle *h, int backlog) {
    coroutine *self = coroutine_self();
    if (!self || coroutine_get(h) != self) {
        sk_error("coroutine_tcp_listen() can only be called inside its own coroutine.");
        return -ESRCH;
    }

    return mgr->tcp_listen(h, backlog);
}

int coroutine_tcp_accept(coroutine_tcp_handle *h, coroutine_tcp_handle *client) {
    coroutine *self = coroutine_self();
    if (!self || coroutine_get(h) != self) {
        sk_error("coroutine_tcp_accept() can only be called inside its own coroutine.");
        return -ESRCH;
    }

    return mgr->tcp_accept(h, client);
}

int coroutine_tcp_shutdown(coroutine_tcp_handle *h) {
    coroutine *self = coroutine_self();
    if (!self || coroutine_get(h) != self) {
        sk_error("coroutine_tcp_shutdown() can only be called inside its own coroutine.");
        return -ESRCH;
    }

    return mgr->tcp_shutdown(h);
}

ssize_t coroutine_tcp_read(coroutine_tcp_handle *h, void *buf, size_t len) {
    coroutine *self = coroutine_self();
    if (!self || coroutine_get(h) != self) {
        sk_error("coroutine_tcp_read() can only be called inside its own coroutine.");
        return -ESRCH;
    }

    return mgr->tcp_read(h, buf, len);
}

ssize_t coroutine_tcp_write(coroutine_tcp_handle *h, const void *buf, size_t len) {
    coroutine *self = coroutine_self();
    if (!self || coroutine_get(h) != self) {
        sk_error("coroutine_tcp_write() can only be called inside its own coroutine.");
        return -ESRCH;
    }

    return mgr->tcp_write(h, buf, len);
}

NS_END(sk)
