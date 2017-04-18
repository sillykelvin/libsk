#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include "reactor_epoll.h"
#include "utility/assert_helper.h"

NS_BEGIN(sk)

reactor_epoll *reactor_epoll::create() {
    reactor_epoll *r = new reactor_epoll();
    if (!r) return nullptr;

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) {
        delete r;
        return nullptr;
    }

    r->epfd_ = epfd;
    return r;
}

reactor_epoll::~reactor_epoll() {
    if (epfd_ != -1) {
        ::close(epfd_);
        epfd_ = -1;
    }
    // TODO: process contexts_ here?
}

bool reactor_epoll::event_registered(const handler_ptr& h, int event_flag) {
    auto it = contexts_.find(h->handle());
    if (it == contexts_.end()) return false;

    return (it->second.flag & event_flag);
}

int reactor_epoll::register_handler(const handler_ptr& h, int event_flag) {
    int op = EPOLL_CTL_ADD;
    auto it = contexts_.find(h->handle());
    if (it != contexts_.end()) {
        event_flag |= it->second.flag;
        if (likely(it->second.flag != EVENT_NONE))
            op = EPOLL_CTL_MOD;
    }

    struct epoll_event e;
    memset(&e, 0x00, sizeof(e));
    if (event_flag & EVENT_READABLE) e.events |= EPOLLIN;
    if (event_flag & EVENT_WRITABLE) e.events |= EPOLLOUT;
    e.data.fd = h->handle();

    int ret = epoll_ctl(epfd_, op, h->handle(), &e);
    if (ret != 0) return ret;

    if (it != contexts_.end()) it->second.flag = event_flag;
    else contexts_.insert(std::make_pair(h->handle(), context(event_flag, h)));

    return 0;
}

int reactor_epoll::deregister_handler(const handler_ptr& h, int event_flag) {
    auto it = contexts_.find(h->handle());
    if (it == contexts_.end()) return 0;

    int flag = it->second.flag & (~event_flag);
    int op = (flag == EVENT_NONE) ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;

    struct epoll_event e;
    memset(&e, 0x00, sizeof(e));
    if (flag & EVENT_READABLE) e.events |= EPOLLIN;
    if (flag & EVENT_WRITABLE) e.events |= EPOLLOUT;
    e.data.fd = h->handle();

    int ret = epoll_ctl(epfd_, op, h->handle(), &e);
    if (ret != 0) return ret;

    if (flag == EVENT_NONE)
        contexts_.erase(it);
    else
        it->second.flag = flag;

    return 0;
}

int reactor_epoll::dispatch(int timeout) {
    struct epoll_event events[1024];
    int nfds = epoll_wait(epfd_, events, array_len(events), timeout);

    if (nfds == 0) return 0;

    if (nfds == -1) {
        if (errno == EINTR)
            return 0;

        return -1;
    }

    for (int i = 0; i < nfds; ++i) {
        struct epoll_event *e = events + i;
        int handle = e->data.fd;
        assert_continue(handle >= 0);

        auto it = contexts_.find(handle);
        assert_continue(it != contexts_.end());

        int event = 0;
        if (e->events & EPOLLIN)  event |= EVENT_READABLE;
        if (e->events & EPOLLOUT) event |= EVENT_WRITABLE;
        if (e->events & EPOLLERR) event |= EVENT_EPOLLERR;
        if (e->events & EPOLLHUP) event |= EVENT_EPOLLHUP;

        // NOTE: the handler might register/deregister handlers
        // in handler::on_event(...) function, thus the iterator
        // "it" might be invalidated in this call
        handler_ptr h = it->second.h;
        h->on_event(event);
    }

    return nfds;
}

NS_END(sk)
