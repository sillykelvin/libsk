#ifndef REACTOR_H
#define REACTOR_H

#include <unordered_map>
#include "net/handler.h"

NS_BEGIN(sk)

class event_handler;

class reactor {
public:
    static const int EVENT_NONE     = 0x00;
    static const int EVENT_READABLE = 0x01;
    static const int EVENT_WRITABLE = 0x02;
    static const int EVENT_EPOLLERR = 0x04;
    static const int EVENT_EPOLLHUP = 0x08;

    reactor() = default;
    virtual ~reactor() = default;

    virtual bool has_event() const;
    virtual bool has_handler(event_handler *h) const;

    virtual void update_handler(event_handler *h) = 0;

    /**
     * @brief dispatch occurred events to corresponding handlers
     * @param timeout: how long to wait for a event, in milliseconds,
     *                 0 to return immediately, -1 for forever
     * @return the count of occurred events(might be 0), -1 on error
     *         and errno will be set
     */
    virtual int dispatch(int timeout) = 0;

protected:
    std::unordered_map<int, event_handler*> handlers_; // fd -> handler
};

NS_END(sk)

#endif // REACTOR_H
