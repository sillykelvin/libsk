#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include "reactor_epoll.h"
#include "utility/assert_helper.h"

NS_BEGIN(sk)
NS_BEGIN(net)

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

void reactor_epoll::register_handler(const handler_ptr& h) {
    int op = EPOLL_CTL_ADD;
    auto it = handlers_.find(h->fd());
    if (it == handlers_.end()) {
        sk_assert(h->has_event());
    } else {
        sk_assert(it->second.lock() == h);

        if (h->has_event())
            op = EPOLL_CTL_MOD;
        else
            op = EPOLL_CTL_DEL;
    }

    int events = h->events();
    struct epoll_event e;
    memset(&e, 0x00, sizeof(e));
    if (events & handler::EVENT_READABLE) e.events |= EPOLLIN;
    if (events & handler::EVENT_WRITABLE) e.events |= EPOLLOUT;
    e.data.fd = h->fd();

    int ret = epoll_ctl(epfd_, op, h->fd(), &e);
    if (ret != 0) {
        sk_fatal("epoll_ctl error: %s.", strerror(errno));
        return;
    }

    if (it == handlers_.end()) {
        handlers_.insert(std::make_pair(h->fd(), h));
    } else {
        if (op == EPOLL_CTL_DEL)
            handlers_.erase(it);
    }
}

int reactor_epoll::dispatch(int timeout) {
    static struct epoll_event events[1024];
    int nfds = epoll_wait(epfd_, events, array_len(events), timeout);

    if (nfds == 0) return 0;

    if (nfds == -1) {
        if (errno == EINTR)
            return 0;

        return -1;
    }

    for (int i = 0; i < nfds; ++i) {
        struct epoll_event *e = events + i;
        int fd = e->data.fd;
        assert_continue(fd >= 0);

        auto it = handlers_.find(fd);
        assert_continue(it != handlers_.end());

        int events = 0;
        if (e->events & EPOLLIN)  events |= handler::EVENT_READABLE;
        if (e->events & EPOLLOUT) events |= handler::EVENT_WRITABLE;
        if (e->events & EPOLLERR) events |= handler::EVENT_EPOLLERR;
        if (e->events & EPOLLHUP) events |= handler::EVENT_EPOLLHUP;

        // NOTE: the handler might register/deregister handlers
        // in handler::on_event(...) function, thus the iterator
        // "it" might be invalidated in this call
        auto h = it->second.lock();
        if (h) h->on_event(events);
        else sk_error("handler<%d> deleted!", fd);
    }

    return nfds;
}

NS_END(net)
NS_END(sk)
