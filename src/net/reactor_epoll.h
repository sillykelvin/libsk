#ifndef REACTOR_EPOLL_H
#define REACTOR_EPOLL_H

#include <unordered_map>
#include "net/reactor.h"

NS_BEGIN(sk)

class reactor_epoll : public reactor {
public:
    MAKE_NONCOPYABLE(reactor_epoll);

    static reactor_epoll *create();
    virtual ~reactor_epoll();

    virtual void enable_reading(const handler_ptr& h) override;
    virtual void enable_writing(const handler_ptr& h) override;
    virtual void disable_reading(const handler_ptr& h) override;
    virtual void disable_writing(const handler_ptr& h) override;
    virtual void disable_all(const handler_ptr& h) override;
    virtual bool reading_enabled(const handler_ptr& h) const override;
    virtual bool writing_enabled(const handler_ptr& h) const override;

    virtual int dispatch(int timeout) override;

private:
    reactor_epoll() : epfd_(-1) {}

private:
    void enable_event(const handler_ptr& h, int event);
    void disable_event(const handler_ptr& h, int event);

private:
    struct context {
        int event;
        handler_ptr h;

        context(int event, const handler_ptr& h) : event(event), h(h) {}
    };

    int epfd_;
    std::unordered_map<int, context> contexts_; // handle -> context
};

NS_END(sk)

#endif // REACTOR_EPOLL_H
