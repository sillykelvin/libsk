#include "tcp_connection.h"

NS_BEGIN(sk)

tcp_connection_ptr tcp_connection::create(reactor *r,
                                          const socket_ptr& socket,
                                          const inet_address& remote_addr) {
    sk_assert(socket);
    return tcp_connection_ptr(new tcp_connection(r, socket, remote_addr));
}

tcp_connection::~tcp_connection() {
    sk_trace("~tcp_connection(%s)", name_.c_str());
}

ssize_t tcp_connection::send(const void *data, size_t len) {
    if (!socket_) {
        sk_error("invalid socket.");
        return -1;
    }

    // empty output queue, just write directly
    ssize_t nbytes = 0;
    size_t remaining = len;
    if (outgoing_.empty()) {
        nbytes = socket_->send(data, len);
        if (nbytes >= 0) {
            remaining = len - nbytes;
            if (remaining <= 0 && fn_on_write_) {
                fn_on_write_(shared_from_this());
                return nbytes;
            }
        } else {
            // EAGAIN and EWOULDBLOCK can be ignored
            // due to this is a non-blocking socket
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                sk_error("send error<%s>.", strerror(errno));
                return -1;
            }
        }
    }

    sk_assert(remaining > 0 && remaining <= len);

    outgoing_.append(sk::byte_offset<const void>(data, nbytes), remaining);

    if (!handler_->writing_enabled())
        handler_->enable_writing();

    return nbytes;
}

void tcp_connection::recv() {
    if (!socket_) {
        sk_error("invalid socket.");
        return;
    }

    if (!handler_->reading_enabled())
        handler_->enable_reading();
}

void tcp_connection::close() {
    if (!socket_) return;

    if (!outgoing_.empty())
        sk_warn("output queue is NOT empty!");

    handler_->disable_all();

    // TODO: better handling here than just forcly close the socket fd
    socket_.reset();

    if (fn_on_close_)
        fn_on_close_(shared_from_this());
}

void tcp_connection::on_read() {
    static char buf[65536]; // 64KB
    ssize_t nbytes = socket_->recv(buf, sizeof(buf));
    if (nbytes > 0) {
        incoming_.append(buf, nbytes);
        if (fn_on_message_)
            fn_on_message_(shared_from_this(), &incoming_);
    } else if (nbytes == 0) {
        on_close();
    } else {
        int error = socket::get_error(socket_->fd());
        sk_error("recv error: %s.", strerror(error));
    }
}

void tcp_connection::on_write() {
    sk_assert(!outgoing_.empty());
    ssize_t nbytes = socket_->send(outgoing_.peek(), outgoing_.size());
    if (nbytes >= 0) {
        outgoing_.consume(nbytes);
        if (outgoing_.empty()) {
            handler_->disable_writing();
            if (fn_on_write_)
                fn_on_write_(shared_from_this());
        }
    } else {
        int error = socket::get_error(socket_->fd());
        sk_error("send error: %s.", strerror(error));
    }
}

void tcp_connection::on_close() {
    handler_->disable_all();

    // TODO: better handling here than just forcly close the socket fd
    socket_.reset();

    if (fn_on_close_)
        fn_on_close_(shared_from_this());
}

void tcp_connection::on_error() {
    int error = socket::get_error(socket_->fd());
    sk_error("connection error: %s.", strerror(error));
}

NS_END(sk)
