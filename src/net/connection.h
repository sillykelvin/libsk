#ifndef CONNECTION_H
#define CONNECTION_H

#include "net/socket.h"
#include "net/handler.h"

NS_BEGIN(sk)

class connection : public handler {
public:
    MAKE_NONCOPYABLE(connection);

    static handler_ptr create(reactor *r);
    virtual ~connection() = default;

    void set_socket(const socket_ptr& socket);

    int send(const void *data, size_t len);
    int recv();

    virtual void on_event(int event) override;
    virtual int handle() const override { return socket_->fd(); }

private:
    connection(reactor *r) : handler(r) {}

private:
    socket_ptr socket_;
};

NS_END(sk)

#endif // CONNECTION_H
