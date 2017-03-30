#ifndef CONNECTOR_H
#define CONNECTOR_H

#include <string>
#include "net/socket.h"
#include "net/handler.h"
#include "net/inet_address.h"

NS_BEGIN(sk)

class connector;
typedef std::shared_ptr<connector> connector_ptr;

class connector : public handler {
public:
    MAKE_NONCOPYABLE(connector);

    static connector_ptr create(reactor *r);
    virtual ~connector() = default;

    int connect(const inet_address& addr);

    virtual void on_event(int event) override;
    virtual int handle() const override { return socket_->fd(); }

private:
    connector(reactor *r) : handler(r) {}

private:
    socket_ptr socket_;
};

NS_END(sk)

#endif // CONNECTOR_H
