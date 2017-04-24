#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include "net/callback.h"

NS_BEGIN(sk)

class reactor;

class event_handler {
public:
    // TODO: merge these events with epoll events
    static const int EVENT_NONE     = 0x00;
    static const int EVENT_READABLE = 0x01;
    static const int EVENT_WRITABLE = 0x02;
    static const int EVENT_EPOLLERR = 0x04;
    static const int EVENT_EPOLLHUP = 0x08;

    MAKE_NONCOPYABLE(event_handler);

    event_handler(reactor *r, int fd);
    ~event_handler();

    void on_event(int events);

    int fd() const { return fd_; }
    int events() const { return events_; }
    bool has_event() const { return events_ != EVENT_NONE; }

    void enable_reading()  { events_ |=  EVENT_READABLE; update(); }
    void enable_writing()  { events_ |=  EVENT_WRITABLE; update(); }
    void disable_reading() { events_ &= ~EVENT_READABLE; update(); }
    void disable_writing() { events_ &= ~EVENT_WRITABLE; update(); }
    void disable_all()     { events_ = EVENT_NONE; update(); }
    bool reading_enabled() { return events_ & EVENT_READABLE; }
    bool writing_enabled() { return events_ & EVENT_WRITABLE; }

    void set_read_callback (const fn_on_poll_event& fn) { fn_on_read_  = fn; }
    void set_write_callback(const fn_on_poll_event& fn) { fn_on_write_ = fn; }
    void set_close_callback(const fn_on_poll_event& fn) { fn_on_close_ = fn; }
    void set_error_callback(const fn_on_poll_event& fn) { fn_on_error_ = fn; }

private:
    void update();

private:
    const int fd_;
    int events_;
    reactor *reactor_;
    fn_on_poll_event fn_on_read_;
    fn_on_poll_event fn_on_write_;
    fn_on_poll_event fn_on_close_;
    fn_on_poll_event fn_on_error_;
};

NS_END(sk)

#endif // EVENT_HANDLER_H
