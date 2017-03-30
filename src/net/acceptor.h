#ifndef ACCEPTOR_H
#define ACCEPTOR_H

#include <string>
#include "net/socket.h"
#include "net/handler.h"

NS_BEGIN(sk)

class acceptor;
typedef std::shared_ptr<acceptor> acceptor_ptr;

class acceptor : public handler {
public:
    MAKE_NONCOPYABLE(acceptor);
    typedef std::function<void(const socket_ptr&, const inet_address&)> fn_on_connection;

    static acceptor_ptr create(reactor *r, const fn_on_connection& fn);
    virtual ~acceptor() = default;

    int listen(const inet_address& addr, int backlog);

    virtual void on_event(int event) override;
    virtual int handle() const override { return socket_->fd(); }

private:
    acceptor(reactor *r, const fn_on_connection& fn)
        : handler(r), fn_on_connection_(fn) {}

private:
    socket_ptr socket_;
    fn_on_connection fn_on_connection_;
};

NS_END(sk)

#endif // ACCEPTOR_H
