#include <core/tcp_server.h>

#define KEEPALIVE_DELAY  10

NS_BEGIN(sk)

tcp_server::tcp_server(uv_loop_t *loop, int backlog,
                       u16 port, const fn_on_connection& fn)
    : loop_(loop),
      backlog_(backlog),
      addr_(port),
      fn_on_connection_(fn) {
    memset(&handle_, 0x00, sizeof(handle_));
    handle_.tcp.data = this;
}

tcp_server::~tcp_server() {
    if (sk_trace_enabled())
        sk_trace("~tcp_server(%s, %lu)", addr_.as_string().c_str());

    if (!connections_.empty()) {
        sk_error("dtor called of unstopped server: %s", addr_.as_string().c_str());
        for (const auto& conn : connections_)
            conn->close();
    }

    if (uv_is_active(&handle_.handle)) {
        sk_error("dtor called of listening server: %s", addr_.as_string().c_str());
        uv_close(&handle_.handle, nullptr);
    }
}

int tcp_server::start() {
    int ret = 0;

    ret = uv_tcp_init(loop_, &handle_.tcp);
    if (ret != 0) {
        sk_error("uv_tcp_init: %s", uv_err_name(ret));
        return ret;
    }

    ret = uv_tcp_keepalive(&handle_.tcp, 1, KEEPALIVE_DELAY);
    if (ret != 0) {
        sk_error("uv_tcp_keepalive: %s", uv_err_name(ret));
        return ret;
    }

    // TODO: set flags here??
    ret = uv_tcp_bind(&handle_.tcp, addr_.address(), 0);
    if (ret != 0) {
        sk_error("uv_tcp_bind: %s", uv_err_name(ret));
        return ret;
    }

    ret = uv_listen(&handle_.stream, backlog_, on_accept);
    if (ret != 0) {
        sk_error("uv_listen: %s", uv_err_name(ret));
        return ret;
    }

    return 0;
}

void tcp_server::stop() {
    for (const auto& conn : connections_)
        conn->close();

    uv_close(&handle_.handle, nullptr);
}

void tcp_server::remove_connection(const tcp_connection_ptr& conn) {
    auto it = connections_.find(conn);
    if (it == connections_.end()) {
        sk_warn("cannot find connection: %s", conn->name().c_str());
        return;
    }

    connections_.erase(it);
    if (fn_on_close_) fn_on_close_(conn);
}

void tcp_server::on_accept(int error) {
    if (error != 0) {
        sk_error("on_accept: %s", uv_err_name(error));
        return;
    }

    uv_tcp_handle_ptr client(cast_ptr(uv_tcp_handle, malloc(sizeof(uv_tcp_handle))), ::free);
    error = uv_tcp_init(loop_, &client->tcp);
    if (error != 0) {
        sk_error("uv_tcp_init: %s", uv_err_name(error));
        return;
    }

    error = uv_accept(&handle_.stream, &client->stream);
    if (error != 0) {
        sk_error("uv_accept: %s", uv_err_name(error));
        return;
    }

    struct sockaddr_in addr;
    int addrlen = static_cast<int>(sizeof(addr));
    error = uv_tcp_getpeername(&client->tcp, cast_ptr(sockaddr, &addr), &addrlen);
    if (error != 0) {
        sk_error("uv_tcp_getpeername: %s", uv_err_name(error));
        return;
    }

    tcp_connection_ptr conn(new tcp_connection(loop_, client, inet_address(addr),
                                               std::bind(&tcp_server::remove_connection,
                                                         this, std::placeholders::_1)));
    conn->set_read_callback(fn_on_read_);
    conn->set_write_callback(fn_on_write_);

    connections_.insert(conn);
    fn_on_connection_(0, conn);
}

void tcp_server::on_accept(uv_stream_t *server, int error) {
    tcp_server *s = cast_ptr(tcp_server, server->data);
    sk_assert(&s->handle_.stream == server);
    return s->on_accept(error);
}

NS_END(sk)
