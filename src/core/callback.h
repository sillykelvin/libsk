#ifndef CALLBACK_H
#define CALLBACK_H

#include <map>
#include <uv.h>
#include <vector>
#include <memory>
#include <functional>
#include <sys/signalfd.h>
#include <utility/types.h>

NS_BEGIN(sk)

class buffer;
class heap_timer;
class tcp_connection;

union uv_tcp_handle {
    uv_handle_t handle;
    uv_stream_t stream;
    uv_tcp_t    tcp;
};
static_assert(sizeof(uv_tcp_handle) == sizeof(uv_tcp_t), "invalid handle");

using tcp_connection_ptr = std::shared_ptr<tcp_connection>;
using uv_tcp_handle_ptr = std::shared_ptr<uv_tcp_handle>;

using fn_on_connection = std::function<void(int, const tcp_connection_ptr&)>;
using fn_on_read = std::function<void(int, const tcp_connection_ptr&, buffer*)>;
using fn_on_write = std::function<void(int, const tcp_connection_ptr&)>;
using fn_on_close = std::function<void(const tcp_connection_ptr&)>;

using fn_on_file_event = std::function<void(const std::string& file)>;
using fn_on_signal_event = std::function<void(const signalfd_siginfo*)>;

using fn_on_timeout = std::function<void(heap_timer*)>;
using fn_on_timer_close = std::function<void(heap_timer*)>;

using string_map = std::map<std::string, std::string>;
using fn_on_http_response = std::function<void(int ret, int http_status, const string_map& headers, const std::string& body)>;

using string_vector = std::vector<std::string>;
using fn_on_consul_watch = std::function<void(int error, int index, const string_map *kv_list)>;
using fn_on_consul_set = std::function<void(int error, const std::string& key, const std::string& value)>;
using fn_on_consul_del = std::function<void(int error, bool recursive, const std::string& key)>;

NS_END(sk)

#endif // CALLBACK_H
