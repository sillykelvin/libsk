#include "event_handler.h"
#include "net/reactor.h"
#include "utility/assert_helper.h"

NS_BEGIN(sk)

event_handler::event_handler(reactor *r, int fd)
    : fd_(fd), events_(EVENT_NONE), reactor_(r) {}

event_handler::~event_handler() {
    sk_assert(!reactor_->has_handler(this));
}

void event_handler::on_event(int events) {
    if ((events & EVENT_EPOLLHUP) && !(events & EVENT_READABLE)) {
        if (fn_on_close_) fn_on_close_();
    }

    if (events & EVENT_EPOLLERR) {
        if (fn_on_error_) fn_on_error_();
    }

    if (events & EVENT_READABLE) {
        if (fn_on_read_) fn_on_read_();
    }

    if (events & EVENT_WRITABLE) {
        if (fn_on_write_) fn_on_write_();
    }
}

void event_handler::update() {
    reactor_->update_handler(this);
}

NS_END(sk)
