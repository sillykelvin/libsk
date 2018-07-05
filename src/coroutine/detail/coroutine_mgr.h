#ifndef COROUTINE_MGR_H
#define COROUTINE_MGR_H

#include <uv.h>
#include <queue>
#include <unordered_set>
#include <unordered_map>
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

    coroutine *get(coroutine_handle *h);
    void bind(coroutine_handle *h, coroutine *c);

    coroutine_handle *handle_create(int type, coroutine *c);
    void handle_close(coroutine_handle *h);
    void handle_free(coroutine_handle *h);
    int handle_type(coroutine_handle *h);

    int tcp_bind(coroutine_handle *h, u16 port);
    int tcp_connect(coroutine_handle *h, const std::string& host, u16 port);
    int tcp_listen(coroutine_handle *h, int backlog);
    int tcp_accept(coroutine_handle *h, coroutine_handle *client);
    int tcp_shutdown(coroutine_handle *h);
    ssize_t tcp_read(coroutine_handle *h, void *buf, size_t len);
    ssize_t tcp_write(coroutine_handle *h, const void *buf, size_t len);

    int fs_add_watch(coroutine_handle *h, const std::string& file);
    int fs_rm_watch(coroutine_handle *h, const std::string& file);
    int fs_watch(coroutine_handle *h, std::vector<coroutine_fs_event> *events);

    static void context_main(intptr_t arg);

private:
    static void yield(coroutine *c);
    static void resume(coroutine *c);

    static void on_sleep_timeout(uv_timer_t *handle);

    static void on_uv_close(uv_handle_t *handle);

    static void on_tcp_connect(uv_connect_t *req, int status);
    static void on_tcp_listen(uv_stream_t *server, int status);
    static void on_tcp_shutdown(uv_shutdown_t *req, int status);
    static void on_tcp_alloc(uv_handle_t *handle, size_t size_hint, uv_buf_t *buf);
    static void on_tcp_read(uv_stream_t *stream, ssize_t nbytes, const uv_buf_t *buf);
    static void on_tcp_write(uv_write_t *req, int status);

    static void on_fs_event(uv_poll_t *handle, int status, int events);

private:
    uv_loop_t *loop_;
    coroutine *uv_;
    coroutine *current_;
    std::queue<coroutine*> runnable_;
};

NS_END(detail)
NS_END(sk)

#endif // COROUTINE_MGR_H
