#ifndef CONNECTION_H
#define CONNECTION_H

#include "net/socket.h"
#include "net/buffer.h"
#include "net/handler.h"

NS_BEGIN(sk)

class connection;
typedef std::shared_ptr<connection> connection_ptr;

class connection : public handler {
public:
    MAKE_NONCOPYABLE(connection);
    typedef std::function<void(const connection_ptr&, buffer*)> fn_on_read;
    typedef std::function<void(const connection_ptr&)> fn_on_write;
    typedef std::function<void(const connection_ptr&)> fn_on_close;

    static connection_ptr create(reactor *r,
                                 const socket_ptr& socket,
                                 const inet_address& remote_addr);
    virtual ~connection() = default;

    ssize_t send(const void *data, size_t len);
    int recv();
    void close();

    const inet_address& remote_address() const { return remote_addr_; }

    void set_read_callback (const fn_on_read&  fn) { fn_on_read_  = fn; }
    void set_write_callback(const fn_on_write& fn) { fn_on_write_ = fn; }
    void set_close_callback(const fn_on_close& fn) { fn_on_close_ = fn; }

    virtual void on_event(int event) override;
    virtual int handle() const override { return socket_->fd(); }

private:
    connection(reactor *r,
               const socket_ptr& socket,
               const inet_address& remote_addr)
        : handler(r), socket_(socket), remote_addr_(remote_addr) {}

private:
    socket_ptr socket_;
    inet_address remote_addr_;
    buffer incoming_;
    buffer outgoing_;

    fn_on_read  fn_on_read_;
    fn_on_write fn_on_write_;
    fn_on_close fn_on_close_;
};

NS_END(sk)

#endif // CONNECTION_H
