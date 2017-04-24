#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include "net/tcp_connection.h"

NS_BEGIN(sk)

class tcp_client;
typedef std::shared_ptr<tcp_client> tcp_client_ptr;

class tcp_client {
public:
    MAKE_NONCOPYABLE(tcp_client);

    static tcp_client_ptr create(reactor *r, const std::string& host, u16 port,
                                 const fn_on_connection_event& fn_on_connection,
                                 const fn_on_error_event& fn_on_error);
    ~tcp_client();

    int connect();
    void remove_connection(const tcp_connection_ptr& conn);

    void set_message_callback(const fn_on_connection_message& fn) { fn_on_message_ = fn; }
    void set_write_callback(const fn_on_connection_event& fn) { fn_on_write_ = fn; }

private:
    tcp_client(reactor *r, const std::string& host, u16 port,
               const fn_on_connection_event& fn_on_connection,
               const fn_on_error_event& fn_on_error)
        : state_(state_disconnected), reactor_(r),
          addr_(host, port), socket_(socket::create()),
          handler_(new event_handler(r, socket_->fd())),
          fn_on_connection_(fn_on_connection), fn_on_error_(fn_on_error) {
        handler_->set_write_callback(std::bind(&tcp_client::on_connect, this));
        handler_->set_error_callback(std::bind(&tcp_client::on_error, this));
    }

private:
    void on_connect();
    void on_error();

private:
    enum state {
        state_connecting,
        state_connected,
        state_disconnected
    };

    state state_;
    reactor *reactor_;
    inet_address addr_;
    socket_ptr socket_;
    tcp_connection_ptr connection_;
    std::unique_ptr<event_handler> handler_;
    fn_on_connection_event fn_on_connection_;
    fn_on_error_event fn_on_error_;

    // optional callbacks
    fn_on_connection_message fn_on_message_;
    fn_on_connection_event fn_on_write_;
};

NS_END(sk)

#endif // TCP_CLIENT_H
