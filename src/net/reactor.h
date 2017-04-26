#ifndef REACTOR_H
#define REACTOR_H

#include <unordered_map>
#include "net/handler.h"

NS_BEGIN(sk)
NS_BEGIN(net)

class reactor {
public:
    reactor() = default;
    virtual ~reactor() = default;

    virtual bool has_pending_event() const { return !handlers_.empty(); }

    virtual void register_handler(const handler_ptr& h) = 0;

    /**
     * @brief dispatch occurred events to corresponding handlers
     * @param timeout: how long to wait for a event, in milliseconds,
     *                 0 to return immediately, -1 for forever
     * @return the count of occurred events(might be 0), -1 on error
     *         and errno will be set
     */
    virtual int dispatch(int timeout) = 0;

protected:
    typedef std::weak_ptr<handler> weak_handler_ptr;
    std::unordered_map<int, weak_handler_ptr> handlers_; // fd -> handler
};

NS_END(net)
NS_END(sk)

#endif // REACTOR_H
