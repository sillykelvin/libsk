#ifndef CONNECTOR_H
#define CONNECTOR_H

#include <string>
#include "net/socket.h"
#include "net/handler.h"

NS_BEGIN(sk)

class connector : public handler {
public:
    MAKE_NONCOPYABLE(connector);

    static handler_ptr create(reactor *r);
    virtual ~connector() = default;

    int connect(const std::string& addr, u16 port);

    virtual void on_event(int event) override;
    virtual int handle() const override { return socket_->fd(); }

private:
    connector(reactor *r) : handler(r) {}

private:
    socket_ptr socket_;
};

NS_END(sk)

#endif // CONNECTOR_H
