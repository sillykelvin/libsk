#ifndef REACTOR_EPOLL_H
#define REACTOR_EPOLL_H

#include "net/reactor.h"

NS_BEGIN(sk)
NS_BEGIN(net)

class reactor_epoll : public reactor {
public:
    MAKE_NONCOPYABLE(reactor_epoll);

    static reactor_epoll *create();
    virtual ~reactor_epoll();

    virtual void register_handler(detail::handler *h) override;

    virtual int dispatch(int timeout) override;

private:
    reactor_epoll() : epfd_(-1) {}

private:
    int epfd_;
};

NS_END(net)
NS_END(sk)

#endif // REACTOR_EPOLL_H
