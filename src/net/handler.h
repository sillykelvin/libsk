#ifndef HANDLER_H
#define HANDLER_H

#include "net/callback.h"

NS_BEGIN(sk)
NS_BEGIN(net)

class reactor;

class handler : public std::enable_shared_from_this<handler> {
public:
    // TODO: merge these events with epoll events
    static const int EVENT_NONE     = 0x00;
    static const int EVENT_READABLE = 0x01;
    static const int EVENT_WRITABLE = 0x02;
    static const int EVENT_EPOLLERR = 0x04;
    static const int EVENT_EPOLLHUP = 0x08;

    MAKE_NONCOPYABLE(handler);

    ~handler();

    void on_read_event();
    void on_write_event();
    void on_error_event();

    int fd() const { return fd_; }
    int events() const { return events_; }
    bool has_event() const { return events_ != EVENT_NONE; }

    void invalidate() { disable_all(); fd_ = -1; }

    void enable_reading()  { enable(EVENT_READABLE);  }
    void enable_writing()  { enable(EVENT_WRITABLE);  }
    void disable_reading() { disable(EVENT_READABLE); }
    void disable_writing() { disable(EVENT_WRITABLE); }
    void disable_all()     { disable(EVENT_READABLE | EVENT_WRITABLE); }
    bool reading_enabled() const { return events_ & EVENT_READABLE; }
    bool writing_enabled() const { return events_ & EVENT_WRITABLE; }

    void set_read_callback (const fn_on_event& fn) { fn_on_read_ = fn;  }
    void set_write_callback(const fn_on_event& fn) { fn_on_write_ = fn; }
    void set_error_callback(const fn_on_event& fn) { fn_on_error_ = fn; }

private:
    handler(reactor *r, int fd);

    void enable(int event);
    void disable(int event);
    void update();

private:
    int fd_;
    int events_;
    reactor *reactor_;
    fn_on_event fn_on_read_;
    fn_on_event fn_on_write_;
    fn_on_event fn_on_error_;

    friend class tcp_client;
    friend class tcp_server;
    friend class tcp_connection;
};
typedef std::shared_ptr<handler> handler_ptr;

NS_END(net)
NS_END(sk)

#endif // HANDLER_H
