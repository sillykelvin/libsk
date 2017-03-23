#ifndef REACTOR_H
#define REACTOR_H

#include "handler.h"

NS_BEGIN(sk)

class reactor {
public:
    static const int READABLE  = 0x01;
    static const int WRITEABLE = 0x02;

    reactor() = default;
    virtual ~reactor() = default;

    virtual int register_handler(const handler_ptr& h, int event_flag) = 0;
    virtual void deregister_handler(const handler_ptr& h, int event_flag) = 0;

    virtual void dispatch(int timeout) = 0;
};

NS_END(sk)

#endif // REACTOR_H
