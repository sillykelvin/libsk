#ifndef CALLBACK_H
#define CALLBACK_H

#include <functional>
#include <memory>
#include "utility/types.h"

NS_BEGIN(sk)
NS_BEGIN(net)

class buffer;
class tcp_connection;
typedef std::shared_ptr<tcp_connection> tcp_connection_ptr;

typedef std::function<void()> fn_on_poll_event;
typedef std::function<void(int)> fn_on_error_event;
typedef std::function<void(const tcp_connection_ptr&)> fn_on_connection_event;
typedef std::function<void(const tcp_connection_ptr&, buffer*)> fn_on_connection_message;

NS_END(net)
NS_END(sk)

#endif // CALLBACK_H
