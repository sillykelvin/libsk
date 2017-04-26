#include "handler.h"
#include "net/reactor.h"
#include "utility/assert_helper.h"

NS_BEGIN(sk)
NS_BEGIN(net)

handler::handler(reactor *r, int fd)
    : fd_(fd), events_(EVENT_NONE), reactor_(r) {}

handler::~handler() {
    sk_assert(!has_event());
}

void handler::on_event(int events) {
    sk_trace("RD(%d) WR(%d) ERR(%d) HUP(%d)",
             events & EVENT_READABLE ? 1 : 0, events & EVENT_WRITABLE ? 1 : 0,
             events & EVENT_EPOLLERR ? 1 : 0, events & EVENT_EPOLLHUP ? 1 : 0);

    if (fd_ == -1) {
        sk_error("handler already invalidated!");
        disable_all();
        return;
    }

    if (events & (EVENT_READABLE | EVENT_EPOLLHUP | EVENT_EPOLLERR)) {
        if (fn_on_read_) fn_on_read_();
        if (fd_ == -1) return;
    }

    if (events & (EVENT_WRITABLE | EVENT_EPOLLHUP | EVENT_EPOLLERR)) {
        if (fn_on_write_) fn_on_write_();
        if (fd_ == -1) return;
    }
}

void handler::enable(int event) {
    if (fd_ == -1) {
        sk_error("handler invalidated!");
        return;
    }

    if (events_ & event)
        return;

    events_ |= event;
    update();
}

void handler::disable(int event) {
    if (fd_ == -1) {
        sk_error("handler invalidated!");
        return;
    }

    if (!(events_ & event))
        return;

    events_ &= ~event;
    update();
}

void handler::update() {
    reactor_->register_handler(shared_from_this());
}

NS_END(net)
NS_END(sk)
