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

typedef std::function<void()> fn_on_event;
typedef std::function<void(int, const tcp_connection_ptr&)> fn_on_connection;
typedef std::function<void(int, const tcp_connection_ptr&, buffer*)> fn_on_read;
typedef std::function<void(int, const tcp_connection_ptr&)> fn_on_write;
typedef std::function<void(const tcp_connection_ptr&)> fn_on_close;

NS_END(net)
NS_END(sk)

#endif // CALLBACK_H
