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

void handler::on_read_event() {
    if (fd_ == -1) {
        sk_error("handler already invalidated!");
        return;
    }

    if (fn_on_read_) fn_on_read_();
}

void handler::on_write_event() {
    if (fd_ == -1) {
        sk_error("handler already invalidated!");
        return;
    }

    if (fn_on_write_) fn_on_write_();
}

void handler::on_error_event() {
    if (fd_ == -1) {
        sk_error("handler already invalidated!");
        return;
    }

    if (fn_on_error_) fn_on_error_();
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
