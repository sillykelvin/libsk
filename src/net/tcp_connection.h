#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include "net/buffer.h"
#include "net/socket.h"
#include "net/event_handler.h"

NS_BEGIN(sk)

class tcp_connection : public std::enable_shared_from_this<tcp_connection> {
public:
    MAKE_NONCOPYABLE(tcp_connection);

    static tcp_connection_ptr create(reactor *r,
                                     const socket_ptr& socket,
                                     const inet_address& remote_addr);
    ~tcp_connection();

    ssize_t send(const void *data, size_t len);
    void recv();
    void close();

    const std::string& name() const { return name_; }
    const inet_address& remote_address() const { return remote_addr_; }

    void set_message_callback(const fn_on_connection_message& fn) { fn_on_message_ = fn; }
    void set_write_callback(const fn_on_connection_event& fn) { fn_on_write_ = fn; }
    void set_close_callback(const fn_on_connection_event& fn) { fn_on_close_ = fn; }

private:
    tcp_connection(reactor *r,
                   const socket_ptr& socket,
                   const inet_address& remote_addr)
        : reactor_(r), socket_(socket),
          remote_addr_(remote_addr),
          handler_(new event_handler(r, socket->fd())) {
        char buf[64] = {0};
        snprintf(buf, sizeof(buf), "[%d->%s]", socket_->fd(), remote_addr_.to_string().c_str());
        name_ = buf;

        handler_->set_read_callback(std::bind(&tcp_connection::on_read, this));
        handler_->set_write_callback(std::bind(&tcp_connection::on_write, this));
        handler_->set_close_callback(std::bind(&tcp_connection::on_close, this));
        handler_->set_error_callback(std::bind(&tcp_connection::on_error, this));
    }

private:
    void on_read();
    void on_write();
    void on_close();
    void on_error();

private:
    reactor *reactor_;
    socket_ptr socket_;
    inet_address remote_addr_;
    std::string name_;
    std::unique_ptr<event_handler> handler_;

    buffer incoming_;
    buffer outgoing_;

    fn_on_connection_message fn_on_message_;
    fn_on_connection_event fn_on_write_;
    fn_on_connection_event fn_on_close_;
};

NS_END(sk)

#endif // TCP_CONNECTION_H
