#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <unordered_set>
#include <core/callback.h>
#include <core/inet_address.h>
#include <core/tcp_connection.h>

NS_BEGIN(sk)

class tcp_server {
public:
    MAKE_NONCOPYABLE(tcp_server);

    tcp_server(uv_loop_t *loop, int backlog, u16 port, const fn_on_connection& fn);
    ~tcp_server();

    int start();
    void stop();

    void set_read_callback (const fn_on_read&  fn) { fn_on_read_  = fn; }
    void set_write_callback(const fn_on_write& fn) { fn_on_write_ = fn; }
    void set_close_callback(const fn_on_close& fn) { fn_on_close_ = fn; }

private:
    void remove_connection(const tcp_connection_ptr& conn);

    void on_accept(int error);
    static void on_accept(uv_stream_t *server, int error);

private:
    uv_loop_t *loop_;
    const int backlog_;
    inet_address addr_;
    uv_tcp_handle handle_;
    fn_on_connection fn_on_connection_;
    std::unordered_set<tcp_connection_ptr> connections_;

    // optional callbacks
    fn_on_read  fn_on_read_;
    fn_on_write fn_on_write_;
    fn_on_close fn_on_close_;
};

NS_END(sk)

#endif // TCP_SERVER_H
