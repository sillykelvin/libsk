#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <string>
#include <core/callback.h>
#include <core/inet_address.h>
#include <core/tcp_connection.h>

NS_BEGIN(sk)

class tcp_client {
public:
    MAKE_NONCOPYABLE(tcp_client);

    tcp_client(uv_loop_t *loop, const std::string& host, u16 port, const fn_on_connection& fn);
    ~tcp_client();

    int connect();
    void stop();

    bool connecting() const { return state_ == state_connecting; }
    bool disconnected() const { return state_ == state_disconnected; }

    void set_read_callback (const fn_on_read&  fn) { fn_on_read_  = fn; }
    void set_write_callback(const fn_on_write& fn) { fn_on_write_ = fn; }
    void set_close_callback(const fn_on_close& fn) { fn_on_close_ = fn; }

private:
    void remove_connection(const tcp_connection_ptr& conn);

    void on_connect(int error);
    static void on_connect(uv_connect_t *req, int status);

private:
    enum state { state_connecting, state_connected, state_disconnected };

    state state_;
    uv_loop_t *loop_;
    inet_address addr_;
    uv_tcp_handle_ptr handle_;
    uv_connect_t connect_req_;
    tcp_connection_ptr connection_;
    fn_on_connection fn_on_connection_;

    // optional callbacks
    fn_on_read  fn_on_read_;
    fn_on_write fn_on_write_;
    fn_on_close fn_on_close_;
};

NS_END(sk)

#endif // TCP_CLIENT_H
