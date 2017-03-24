#ifndef ACCEPTOR_H
#define ACCEPTOR_H

#include <string>
#include "net/socket.h"
#include "net/handler.h"

NS_BEGIN(sk)

class acceptor : public handler {
public:
    MAKE_NONCOPYABLE(acceptor);

    static handler_ptr create(reactor *r);
    virtual ~acceptor() = default;

    int listen(const std::string& addr, u16 port, int backlog);

    virtual void on_event(int event) override;
    virtual int handle() const override { return socket_->fd(); }

private:
    acceptor(reactor *r) : handler(r) {}

private:
    socket_ptr socket_;
};

NS_END(sk)

#endif // ACCEPTOR_H
