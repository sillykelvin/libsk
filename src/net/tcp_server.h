#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <unordered_set>
#include "net/tcp_connection.h"

NS_BEGIN(sk)
NS_BEGIN(net)

class tcp_server {
public:
    MAKE_NONCOPYABLE(tcp_server);

    tcp_server(reactor *r, int backlog, u16 port, const fn_on_connection& fn);
    ~tcp_server();

    int start();
    void stop();

    void set_read_callback (const fn_on_read&  fn) { fn_on_read_  = fn; }
    void set_write_callback(const fn_on_write& fn) { fn_on_write_ = fn; }

private:
    void remove_connection(const tcp_connection_ptr& conn);
    void on_accept();

private:
    reactor *reactor_;
    const int backlog_;
    inet_address addr_;
    socket_ptr socket_;
    fn_on_connection fn_on_connection_;
    handler_ptr handler_;
    std::unordered_set<tcp_connection_ptr> connections_;

    // optional callbacks
    fn_on_read fn_on_read_;
    fn_on_write fn_on_write_;
};

NS_END(net)
NS_END(sk)

#endif // TCP_SERVER_H
