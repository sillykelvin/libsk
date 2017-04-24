#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <set>
#include "net/tcp_connection.h"

NS_BEGIN(sk)

class tcp_server;
typedef std::shared_ptr<tcp_server> tcp_server_ptr;

class tcp_server {
public:
    MAKE_NONCOPYABLE(tcp_server);

    static tcp_server_ptr create(reactor *r, int backlog, u16 port,
                                 const fn_on_connection_event& fn_on_connection);
    ~tcp_server();

    int start();
    void remove_connection(const tcp_connection_ptr& conn);

    void set_message_callback(const fn_on_connection_message& fn) { fn_on_message_ = fn; }
    void set_write_callback(const fn_on_connection_event& fn) { fn_on_write_ = fn; }

private:
    tcp_server(reactor *r, int backlog, u16 port,
               const fn_on_connection_event& fn_on_connection)
        : reactor_(r), backlog_(backlog),
          addr_(port), socket_(socket::create()),
          handler_(new event_handler(r, socket_->fd())),
          fn_on_connection_(fn_on_connection) {
        handler_->set_read_callback(std::bind(&tcp_server::on_accept, this));
    }

private:
    void on_accept();

private:
    reactor *reactor_;
    const int backlog_;
    inet_address addr_;
    socket_ptr socket_;
    std::unique_ptr<event_handler> handler_;
    fn_on_connection_event fn_on_connection_;
    std::set<tcp_connection_ptr> connections_;

    // optional callbacks
    fn_on_connection_message fn_on_message_;
    fn_on_connection_event fn_on_write_;
};

NS_END(sk)

#endif // TCP_SERVER_H
