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

    /**
     * @brief event_registered will return whether the
     *        specified event registered or not
     * @param h: the handler
     * @param event_flag: the event(s) to be queried
     * @return true if registered, false if not
     */
    virtual bool event_registered(const handler_ptr& h, int event_flag) = 0;

    /**
     * @brief register_handler will register the handler h
     *        on events specified by event_flag
     * @param h: the handler
     * @param event_flag: the ORed events
     * @return 0 if succeeds, -1 on error and errno will be set
     */
    virtual int register_handler(const handler_ptr& h, int event_flag) = 0;

    /**
     * @brief deregister_handler will remove the handler h
     *        on events specified by event_flag
     * @param h: the handler
     * @param event_flag: the ORed events
     * @return 0 if succeeds, -1 on error and errno will be set
     */
    virtual int deregister_handler(const handler_ptr& h, int event_flag) = 0;

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
