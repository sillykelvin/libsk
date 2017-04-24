#include "reactor.h"
#include "net/event_handler.h"

NS_BEGIN(sk)

bool reactor::has_event() const {
    return !handlers_.empty();
}

bool reactor::has_handler(event_handler *h) const {
    auto it = handlers_.find(h->fd());
    return it != handlers_.end() && it->second == h;
}

NS_END(sk)
