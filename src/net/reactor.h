#ifndef REACTOR_H
#define REACTOR_H

#include <unordered_map>
#include "net/detail/handler.h"

NS_BEGIN(sk)
NS_BEGIN(net)

class reactor {
public:
    reactor() = default;
    virtual ~reactor() = default;

    virtual bool has_pending_event() const { return !handlers_.empty(); }

    virtual bool handler_registered(detail::handler *h) const {
        auto it = handlers_.find(h->fd());
        return it != handlers_.end() && it->second == h;
    }

    virtual void register_handler(detail::handler *h) = 0;

    /**
     * @brief dispatch occurred events to corresponding handlers
     * @param timeout: how long to wait for a event, in milliseconds,
     *                 0 to return immediately, -1 for forever
     * @return the count of occurred events(might be 0), -1 on error
     *         and errno will be set
     */
    virtual int dispatch(int timeout) = 0;

protected:
    std::unordered_map<int, detail::handler*> handlers_; // fd -> handler
};

NS_END(net)
NS_END(sk)

#endif // REACTOR_H
