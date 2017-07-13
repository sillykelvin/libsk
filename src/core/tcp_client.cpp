#include <core/tcp_client.h>

NS_BEGIN(sk)

tcp_client::tcp_client(uv_loop_t *loop, const std::string& host,
                       u16 port, const fn_on_connection& fn)
    : state_(state_disconnected),
      loop_(loop),
      addr_(host, port),
      handle_(cast_ptr(uv_tcp_handle, malloc(sizeof(uv_tcp_handle))), ::free),
      fn_on_connection_(fn) {
    handle_->tcp.data = this;
    memset(&connect_req_, 0x00, sizeof(connect_req_));
    connect_req_.data = this;
}

tcp_client::~tcp_client() {
    if (sk_trace_enabled())
        sk_trace("~tcp_client(%s, %d, %s)",
                 addr_.as_string().c_str(), state_,
                 connection_ ? connection_->name().c_str() : "NA");

    if (state_ == state_connecting) {
        sk_error("dtor called of connecting client: %s", addr_.as_string().c_str());
        uv_close(&handle_->handle, nullptr);
    }

    if (state_ == state_connected && connection_) {
        sk_error("dtor called of connected client: %s", addr_.as_string().c_str());
        connection_->close();
    }
}

int tcp_client::connect() {
    if (state_ != state_disconnected) {
        sk_warn("wrong state: %d", state_);
        return 0;
    }

    int ret = uv_tcp_init(loop_, &handle_->tcp);
    if (ret != 0) {
        sk_error("uv_tcp_init: %s", uv_err_name(ret));
        return ret;
    }

    ret = uv_tcp_connect(&connect_req_, &handle_->tcp, addr_.address(), on_connect);
    if (ret != 0) {
        sk_error("uv_tcp_connect: %s", uv_err_name(ret));
        return ret;
    }

    state_ = state_connecting;
    return 0;
}

void tcp_client::stop() {
    if (state_ == state_connecting) {
        uv_close(&handle_->handle, nullptr);
        state_ = state_disconnected;
        return;
    }

    if (state_ == state_connected) {
        sk_assert(connection_);
        connection_->close();
        return;
    }
}

void tcp_client::remove_connection(const tcp_connection_ptr& conn) {
    sk_assert(conn == connection_);

    state_ = state_disconnected;
    connection_.reset();

    if (fn_on_close_) fn_on_close_(conn);
}

void tcp_client::on_connect(int error) {
    if (error != 0) {
        sk_error("on_connect: %s", uv_err_name(error));
        state_ = state_disconnected;
        return fn_on_connection_(error, nullptr);
    }

    tcp_connection_ptr conn(new tcp_connection(loop_, handle_, addr_,
                                               std::bind(&tcp_client::remove_connection,
                                                         this, std::placeholders::_1)));
    conn->set_read_callback(fn_on_read_);
    conn->set_write_callback(fn_on_write_);

    state_ = state_connected;
    handle_.reset();
    connection_ = conn;
    fn_on_connection_(0, conn);
}

void tcp_client::on_connect(uv_connect_t *req, int status) {
    tcp_client *conn = cast_ptr(tcp_client, req->data);
    sk_assert(&conn->connect_req_ == req);
    sk_assert(&conn->handle_->stream == req->handle);
    return conn->on_connect(status);
}

NS_END(sk)
