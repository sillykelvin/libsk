#ifndef REACTOR_EPOLL_H
#define REACTOR_EPOLL_H

#include <unordered_map>
#include "reactor.h"

NS_BEGIN(sk)

class reactor_epoll : public reactor {
public:
    MAKE_NONCOPYABLE(reactor_epoll);

    static reactor_epoll *create();
    virtual ~reactor_epoll();

    virtual int register_handler(const handler_ptr& h, int event_flag) override;
    virtual int deregister_handler(const handler_ptr& h, int event_flag) override;

    virtual int dispatch(int timeout) override;

private:
    reactor_epoll() : epfd_(-1) {}

private:
    struct context {
        int flag;
        handler_ptr h;

        context(int flag, const handler_ptr& h) : flag(flag), h(h) {}
    };

    int epfd_;
    std::unordered_map<int, context> contexts_; // handle -> context
};

NS_END(sk)

#endif // REACTOR_EPOLL_H
