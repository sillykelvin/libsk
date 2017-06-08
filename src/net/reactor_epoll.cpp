#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include "reactor_epoll.h"
#include "utility/assert_helper.h"

NS_BEGIN(sk)
NS_BEGIN(net)

reactor_epoll *reactor_epoll::create() {
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) return nullptr;

    reactor_epoll *r = new reactor_epoll();
    if (!r) return nullptr;

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

        auto h = it->second.lock();
        if (!h) {
            sk_error("handler<%d> deleted!", fd);
            continue;
        }

        sk_trace("RD(%d) WR(%d) ERR(%d) HUP(%d), RDHUP(%d), fd(%d)",
                 e->events & EPOLLIN ? 1 : 0, e->events & EPOLLOUT ? 1 : 0,
                 e->events & EPOLLERR ? 1 : 0, e->events & EPOLLHUP ? 1 : 0,
                 e->events & EPOLLRDHUP ? 1 : 0, fd);

        if (e->events & EPOLLIN)
            h->on_read_event();

        if (e->events & EPOLLOUT)
            h->on_write_event();

        if (e->events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {
            // ignore the error here as it has been processed
            // by IN and OUT event handler
            if (e->events & (EPOLLIN | EPOLLOUT))
                continue;

            h->on_error_event();
        }
    }

    return nfds;
}

NS_END(net)
NS_END(sk)
