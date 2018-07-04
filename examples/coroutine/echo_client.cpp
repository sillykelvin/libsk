#include <libsk.h>

int main() {
    int ret = sk::logger::init("client_log.xml");
    if (ret != 0) return ret;

    sk::coroutine_init(uv_default_loop());

    for (int i = 0; i < 5; ++i) {
        char buf[32];
        snprintf(buf, sizeof(buf), "conn-%d", i);

        sk::coroutine_create(buf, [i] () {
            sk::coroutine_handle *h = sk::coroutine_handle_create(sk::coroutine_handle_tcp);
            int ret = sk::coroutine_tcp_connect(h, "127.0.0.1", 7777);
            if (ret != 0) {
                sk_error("connect error.");
                sk::coroutine_handle_free(h);
                return;
            }

            sk_trace("=>> write(%s)", sk::coroutine_name());

            ssize_t nwrite = -1;
            if (i == 4) {
                nwrite = sk::coroutine_tcp_write(h, "Q", 1);
            } else {
                nwrite = sk::coroutine_tcp_write(h, "hello", 5);
            }

            sk_trace("<<= write(%s)", sk::coroutine_name());

            if (nwrite < 0) {
                sk_error("write error.");
                sk::coroutine_handle_close(h);
                sk::coroutine_handle_free(h);
                return;
            }

            char buf[32];
            sk_trace("=>> read(%s)", sk::coroutine_name());
            ssize_t nread = sk::coroutine_tcp_read(h, buf, sizeof(buf));
            sk_trace("<<= read(%s)", sk::coroutine_name());

            if (nread == 0) {
                sk_debug("empty read: %s", sk::coroutine_name());
            } else if (nread == UV_EOF) {
                sk_debug("EOF: %s", sk::coroutine_name());
                sk::coroutine_handle_close(h);
                sk::coroutine_handle_free(h);
                return;
            } else if (nread < 0) {
                sk_error("read error: %s", sk::coroutine_name());
                sk::coroutine_handle_close(h);
                sk::coroutine_handle_free(h);
                return;
            } else {
                sk_debug("read content: %s:%ld (%s)", buf, nread, sk::coroutine_name());
            }

            sk_trace("=>> shutdown(%s)", sk::coroutine_name());
            ret = sk::coroutine_tcp_shutdown(h);
            sk_trace("<<= shutdown(%s)", sk::coroutine_name());
            if (ret != 0) {
                sk_error("shutdown error.");
            }

            sk_trace("=>> close(%s)", sk::coroutine_name());
            sk::coroutine_handle_close(h);
            sk_trace("<<= close(%s)", sk::coroutine_name());

            sk::coroutine_handle_free(h);
        });
    }

    sk::coroutine_schedule();
    sk::coroutine_fini();

    sk_debug("main done.");
    return 0;
}
