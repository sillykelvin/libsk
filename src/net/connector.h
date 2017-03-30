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
    typedef std::function<void(const socket_ptr&)> fn_on_connection;

    static connector_ptr create(reactor *r, const fn_on_connection& fn);
    virtual ~connector() = default;

    int connect(const inet_address& addr);

    virtual void on_event(int event) override;
    virtual int handle() const override { return socket_->fd(); }

private:
    connector(reactor *r, const fn_on_connection& fn)
        : handler(r), fn_on_connection_(fn) {}

private:
    socket_ptr socket_;
    fn_on_connection fn_on_connection_;
};

NS_END(sk)

#endif // CONNECTOR_H
