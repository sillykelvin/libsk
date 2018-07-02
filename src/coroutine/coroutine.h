#ifndef COROUTINE_H
#define COROUTINE_H

#include <uv.h>
#include <functional>
#include <utility/types.h>

NS_BEGIN(sk)

struct coroutine;
struct coroutine_tcp_handle;
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

coroutine *coroutine_get(coroutine_tcp_handle *h);
void coroutine_bind(coroutine_tcp_handle *h, coroutine *c);

coroutine_tcp_handle *coroutine_tcp_create(coroutine *c = nullptr);
void coroutine_tcp_free(coroutine_tcp_handle *h);
void coroutine_tcp_close(coroutine_tcp_handle *h);

int coroutine_tcp_bind(coroutine_tcp_handle *h, u16 port);
int coroutine_tcp_connect(coroutine_tcp_handle *h, const std::string& host, u16 port); // TODO: timeout here
int coroutine_tcp_listen(coroutine_tcp_handle *h, int backlog);
int coroutine_tcp_accept(coroutine_tcp_handle *h, coroutine_tcp_handle *client);
int coroutine_tcp_shutdown(coroutine_tcp_handle *h);

ssize_t coroutine_tcp_read(coroutine_tcp_handle *h, void *buf, size_t len); // TODO: timeout here
ssize_t coroutine_tcp_write(coroutine_tcp_handle *h, const void *buf, size_t len); // TODO: timeout here

NS_END(sk)

#endif // COROUTINE_H
