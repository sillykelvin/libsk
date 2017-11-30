#include <core/tcp_connection.h>

NS_BEGIN(sk)

tcp_connection::tcp_connection(uv_loop_t *loop, const uv_tcp_handle_ptr& peer,
                               const inet_address& remote_addr, const fn_on_close& fn)
    : state_(state_connected),
      loop_(loop),
      handle_(peer),
      remote_addr_(remote_addr),
      fn_on_close_(fn) {
    handle_->tcp.data = this;
    update_name();
}

tcp_connection::~tcp_connection() {
    if (sk_trace_enabled()) sk_trace("~tcp_connection(%s)", name_.c_str());
    if (likely(state_ == state_disconnected)) return;

    if (state_ == state_disconnecting) {
        sk_error("dtor called when disconnecting: %s", name_.c_str());
        return;
    }

    if (state_ == state_connected) {
        sk_error("dtor called of connected connection: %s", name_.c_str());
        uv_close(&handle_->handle, nullptr);
        return;
    }
}

int tcp_connection::send(const void *data, size_t len) {
    if (state_ != state_connected) {
        sk_error("invalid connection: %s", name_.c_str());
        return EINVAL;
    }

    write_request_ptr req(new write_request(this, data, len));
    uv_buf_t buf = uv_buf_init(char_ptr(const_cast<void*>(req->buf.peek())), req->buf.size());

    int ret = uv_write(&req->req, &handle_->stream, &buf, 1, on_write);
    if (ret != 0) {
        sk_error("uv_write: %s", uv_err_name(ret));
        return ret;
    }

    outgoing_.push_back(std::move(req));
    return 0;
}

int tcp_connection::recv() {
    if (state_ != state_connected) {
        sk_error("invalid connection: %s", name_.c_str());
        return EINVAL;
    }

    int ret = uv_read_start(&handle_->stream, on_alloc, on_read);
    if (ret != 0) {
        sk_error("uv_read_start: %s", uv_err_name(ret));
        return ret;
    }

    return 0;
}

void tcp_connection::close() {
    if (state_ == state_disconnecting || state_ == state_disconnected)
        return;

    sk_assert(state_ == state_connected);

    if (!outgoing_.empty())
        sk_warn("output queue is NOT empty! connection: %s", name_.c_str());

    sk_trace("close connection: %s", name_.c_str());
    uv_close(&handle_->handle, on_close);
    state_ = state_disconnecting;
    update_name(); // update name as the state changed
}

void tcp_connection::update_name() {
    static_assert(std::is_same<int, uv_os_fd_t>::value, "fd type must be int");
    static char buf[64];

    uv_os_fd_t fd = -1;
    uv_fileno(&handle_->handle, &fd);
    snprintf(buf, sizeof(buf), "[%d, %s, %d]",
             fd, remote_addr_.as_string().c_str(), state_);
    name_ = buf;
}

void tcp_connection::on_alloc(uv_buf_t *buf, size_t size_hint) {
    /*
     * size_hint given by libuv is 65536, which is too large and
     * improper, we resize it to 1024 here, it should be enough
     */
    size_hint = 1024;
    buf->base = char_ptr(incoming_.prepare(size_hint));
    buf->len = size_hint;
}

void tcp_connection::on_alloc(uv_handle_t *handle, size_t size_hint, uv_buf_t *buf) {
    tcp_connection *conn = cast_ptr(tcp_connection, handle->data);
    sk_assert(&conn->handle_->handle == handle);
    return conn->on_alloc(buf, size_hint);
}

void tcp_connection::on_read(int nbytes, const uv_buf_t *buf) {
    unused_parameter(buf);
    if (nbytes == 0) return;

    int error = 0;
    buffer *incoming = nullptr;

    do {
        if (nbytes == UV_EOF) {
            error = READ_EOF;
            incoming = &incoming_;
            break;
        }

        if (nbytes < 0) {
            sk_error("on_read: %s", uv_err_name(nbytes));
            error = nbytes;
            incoming = nullptr;
            break;
        }

        error = 0;
        incoming_.commit(nbytes);
        incoming = &incoming_;
    } while (0);

    if (fn_on_read_) fn_on_read_(error, shared_from_this(), incoming);
}

void tcp_connection::on_read(uv_stream_t *handle, ssize_t nbytes, const uv_buf_t *buf) {
    tcp_connection *conn = cast_ptr(tcp_connection, handle->data);
    sk_assert(&conn->handle_->stream == handle);
    return conn->on_read(static_cast<int>(nbytes), buf);
}

void tcp_connection::on_write(write_request *req, int status) {
    sk_assert(!outgoing_.empty());
    write_request_ptr ptr = std::move(outgoing_.front());
    outgoing_.pop_front();
    sk_assert(ptr.get() == req);

    if (status != 0) sk_error("on_write: %s", uv_err_name(status));
    if (fn_on_write_) fn_on_write_(status, shared_from_this());
}

void tcp_connection::on_write(uv_write_t *req, int status) {
    write_request *request = reinterpret_cast<write_request*>(req);
    tcp_connection *conn = cast_ptr(tcp_connection, req->data);
    return conn->on_write(request, status);
}

void tcp_connection::on_close() {
    state_ = state_disconnected;
    update_name(); // update name as the state changed
    fn_on_close_(shared_from_this());
}

void tcp_connection::on_close(uv_handle_t *handle) {
    tcp_connection *conn = cast_ptr(tcp_connection, handle->data);
    sk_assert(&conn->handle_->handle == handle);
    return conn->on_close();
}

NS_END(sk)
