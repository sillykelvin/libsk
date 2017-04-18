#ifndef REACTOR_H
#define REACTOR_H

#include "net/handler.h"

NS_BEGIN(sk)

class reactor {
public:
    static const int EVENT_NONE     = 0x00;
    static const int EVENT_READABLE = 0x01;
    static const int EVENT_WRITABLE = 0x02;
    static const int EVENT_EPOLLERR = 0x04;
    static const int EVENT_EPOLLHUP = 0x08;

    reactor() = default;
    virtual ~reactor() = default;

    virtual void enable_reading(const handler_ptr& h) = 0;
    virtual void enable_writing(const handler_ptr& h) = 0;
    virtual void disable_reading(const handler_ptr& h) = 0;
    virtual void disable_writing(const handler_ptr& h) = 0;
    virtual bool reading_enabled(const handler_ptr& h) const = 0;
    virtual bool writing_enabled(const handler_ptr& h) const = 0;

    /**
     * @brief dispatch occurred events to corresponding handlers
     * @param timeout: how long to wait for a event, in milliseconds,
     *                 0 to return immediately, -1 for forever
     * @return the count of occurred events(might be 0), -1 on error
     *         and errno will be set
     */
    virtual int dispatch(int timeout) = 0;
};

NS_END(sk)

#endif // REACTOR_H
