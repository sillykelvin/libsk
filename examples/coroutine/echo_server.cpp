#include <libsk.h>

int main() {
    int ret = sk::logger::init("server_log.xml");
    if (ret != 0) return ret;

    sk::coroutine_init(uv_default_loop());

    sk::coroutine_create("server", [] () {
        sk::coroutine_tcp_handle *h = sk::coroutine_tcp_create();
        int ret = sk::coroutine_tcp_bind(h, 7777);
        if (ret != 0) {
            sk_error("bind error.");
            sk::coroutine_tcp_free(h);
            return;
        }

        int total_count = 0;
        int active_count = 0;
        bool exit = false;
        while (!exit) {
            sk_trace("=>> listen");
            ret = sk::coroutine_tcp_listen(h, 10);
            sk_trace("<<= listen");
            if (ret != 0) {
                sk_error("listen error.");
                sk::coroutine_tcp_free(h);
                return;
            }

            char buf[32];
            snprintf(buf, sizeof(buf), "conn-%d", total_count++);

            sk::coroutine_tcp_handle *client = sk::coroutine_tcp_create();
            sk::coroutine *c = sk::coroutine_create(buf, [client, &exit, &active_count] () {
                char buf[4];
                buf[sizeof(buf) - 1] = 0;

                while (true) {
                    sk_trace("=>> read(%s)", sk::coroutine_name());
                    ssize_t nread = sk::coroutine_tcp_read(client, buf, sizeof(buf) - 1);
                    sk_trace("<<= read(%s)", sk::coroutine_name());
                    if (nread == 0) {
                        sk_debug("empty read: %s", sk::coroutine_name());
                    } else if (nread == UV_EOF) {
                        sk_debug("EOF: %s", sk::coroutine_name());
                        break;
                    } else if (nread < 0) {
                        sk_error("read error: %s", sk::coroutine_name());
                        break;
                    } else {
                        sk_debug("read content: %s:%ld (%s)", buf, nread, sk::coroutine_name());

                        sk_trace("=>> write(%s)", sk::coroutine_name());
                        ssize_t nwrite = sk::coroutine_tcp_write(client, buf, cast_size(nread));
                        sk_trace("<<= write(%s)", sk::coroutine_name());

                        if (nread == 1 && buf[0] == 'Q') {
                            exit = true;
                        }

                        if (nwrite < 0) {
                            sk_error("write error: %s", sk::coroutine_name());
                            break;
                        } else {
                            sk_assert(nwrite == nread);
                        }
                    }
                }

                sk_trace("=>> shutdown(%s)", sk::coroutine_name());
                int ret = sk::coroutine_tcp_shutdown(client);
                sk_trace("<<= shutdown(%s)", sk::coroutine_name());
                if (ret != 0) {
                    sk_error("shutdown error.");
                }

                sk_trace("=>> close(%s)", sk::coroutine_name());
                sk::coroutine_tcp_close(client);
                sk_trace("<<= close(%s)", sk::coroutine_name());

                sk::coroutine_tcp_free(client);
                --active_count;
            });

            ++active_count;
            sk::coroutine_bind(client, c);
            ret = sk::coroutine_tcp_accept(h, client);
            if (ret != 0) {
                sk_error("accept error.");
                sk::coroutine_tcp_close(h);
                sk::coroutine_tcp_free(h);
                return;
            }
        }

        // cannot shutdown listen socket, it will block forever
        // sk_trace("=>> shutdown(%s)", sk::coroutine_name());
        // ret = sk::coroutine_tcp_shutdown(h);
        // sk_trace("<<= shutdown(%s)", sk::coroutine_name());
        // if (ret != 0) {
        //     sk_error("shutdown error.");
        // }

        // wait all connection coroutines to finish, otherwise
        // there will be core dump with segfault
        while (active_count > 0) {
            sk::coroutine_sleep(1000);
        }
        sk_assert(active_count == 0);

        sk_trace("=>> close(%s)", sk::coroutine_name());
        sk::coroutine_tcp_close(h);
        sk_trace("<<= close(%s)", sk::coroutine_name());

        sk::coroutine_tcp_free(h);
    });

    sk::coroutine_schedule();
    sk::coroutine_fini();

    sk_debug("main done.");
    return 0;
}
