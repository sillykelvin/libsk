#ifndef COROUTINE_H
#define COROUTINE_H

#include <uv.h>
#include <vector>
#include <functional>
#include <sys/signalfd.h>
#include <utility/types.h>

NS_BEGIN(sk)

enum coroutine_handle_type {
    coroutine_handle_tcp,
    coroutine_handle_fs_watcher,
    coroutine_handle_signal_watcher
};

struct coroutine_fs_event {
    enum event_type {
        event_open   = 0x1,
        event_close  = 0x2,
        event_change = 0x4,
        event_delete = 0x8
    };

    int events;
    std::string file;
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

int coroutine_fs_add_watch(coroutine_handle *h, const std::string& file);
int coroutine_fs_rm_watch(coroutine_handle *h, const std::string& file);
int coroutine_fs_watch(coroutine_handle *h, std::vector<coroutine_fs_event> *events);

int coroutine_sig_add_watch(coroutine_handle *h, int signal);
int coroutine_sig_rm_watch(coroutine_handle *h, int signal);
int coroutine_sig_watch(coroutine_handle *h, std::vector<signalfd_siginfo> *signals);

NS_END(sk)

#endif // COROUTINE_H
