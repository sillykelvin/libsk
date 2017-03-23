#ifndef HANDLER_H
#define HANDLER_H

#include <memory>
#include "utility/types.h"

NS_BEGIN(sk)

class reactor;

class handler {
public:
    handler(reactor *r) : reactor_(r) {}
    virtual ~handler() = default;

    virtual void on_event(int event) = 0;
    virtual int handle() const = 0;

protected:
    reactor *reactor_;
};
typedef std::shared_ptr<handler> handler_ptr;

NS_END(sk)

#endif // HANDLER_H
