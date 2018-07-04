#ifndef COROUTINE_H
#define COROUTINE_H

#include <uv.h>
#include <functional>
#include <utility/types.h>

NS_BEGIN(sk)

enum coroutine_handle_type {
    coroutine_handle_tcp,
    coroutine_handle_fs_watcher,
    coroutine_handle_signal_watcher
};

struct coroutine;
struct coroutine_handle;
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

coroutine *coroutine_get(coroutine_handle *h);
void coroutine_bind(coroutine_handle *h, coroutine *c);

coroutine_handle *coroutine_handle_create(int type, coroutine *c = nullptr);
void coroutine_handle_close(coroutine_handle *h);
void coroutine_handle_free(coroutine_handle *h);

int coroutine_handle_type(coroutine_handle *h);

int coroutine_tcp_bind(coroutine_handle *h, u16 port);
int coroutine_tcp_connect(coroutine_handle *h, const std::string& host, u16 port); // TODO: timeout here
int coroutine_tcp_listen(coroutine_handle *h, int backlog);
int coroutine_tcp_accept(coroutine_handle *h, coroutine_handle *client);
int coroutine_tcp_shutdown(coroutine_handle *h);

// TODO: retrun 0 when EOF instead of UV_EOF (be consistent with the "read()" system call)
ssize_t coroutine_tcp_read(coroutine_handle *h, void *buf, size_t len); // TODO: timeout here
ssize_t coroutine_tcp_write(coroutine_handle *h, const void *buf, size_t len); // TODO: timeout here

NS_END(sk)

#endif // COROUTINE_H
