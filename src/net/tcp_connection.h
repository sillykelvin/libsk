#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include "net/buffer.h"
#include "net/socket.h"
#include "net/handler.h"

NS_BEGIN(sk)
NS_BEGIN(net)

class tcp_connection : public std::enable_shared_from_this<tcp_connection> {
public:
    MAKE_NONCOPYABLE(tcp_connection);

    ~tcp_connection();

    ssize_t send(const void *data, size_t len);
    void recv();
    void close();

    const std::string& name() const { return name_; }
    const inet_address& remote_address() const { return remote_addr_; }

    void set_read_callback (const fn_on_read&  fn) { fn_on_read_  = fn; }
    void set_write_callback(const fn_on_write& fn) { fn_on_write_ = fn; }

private:
    tcp_connection(reactor *r, const socket_ptr& socket,
                   const inet_address& remote_addr, const fn_on_close& fn);

    void on_read();
    void on_write();

private:
    reactor *reactor_;
    socket_ptr socket_;
    inet_address remote_addr_;
    std::string name_;
    fn_on_close fn_on_close_;
    handler_ptr handler_;

    buffer incoming_;
    buffer outgoing_;

    fn_on_read fn_on_read_;
    fn_on_write fn_on_write_;

    friend class tcp_client;
    friend class tcp_server;
};

NS_END(net)
NS_END(sk)

#endif // TCP_CONNECTION_H
