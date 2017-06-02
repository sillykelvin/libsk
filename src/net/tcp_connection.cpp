#include "tcp_connection.h"

NS_BEGIN(sk)
NS_BEGIN(net)

tcp_connection::tcp_connection(reactor *r, const socket_ptr& socket,
                               const inet_address& remote_addr, const fn_on_close& fn)
    : reactor_(r), socket_(socket), remote_addr_(remote_addr),
      fn_on_close_(fn), handler_(new handler(r, socket->fd())) {
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "[%d->%s]", socket_->fd(), remote_addr_.to_string().c_str());
    name_ = buf;

    handler_->set_read_callback(std::bind(&tcp_connection::on_read, this));
    handler_->set_write_callback(std::bind(&tcp_connection::on_write, this));
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
                fn_on_write_(0, shared_from_this());
                return nbytes;
            }
        } else {
            // EAGAIN and EWOULDBLOCK can be ignored
            // due to this is a non-blocking socket
            int error = errno;
            sk_error("send error: %s.", strerror(error));
            if (error != EAGAIN && error != EWOULDBLOCK) {
                fn_on_write_(error, shared_from_this());
                return -1;
            }
        }
    }

    sk_assert(remaining > 0 && remaining <= len);

    outgoing_.append(sk::byte_offset<const void>(data, nbytes), remaining);
    handler_->enable_writing();
    return nbytes;
}

void tcp_connection::recv() {
    if (!socket_) {
        sk_error("invalid socket.");
        return;
    }

    handler_->enable_reading();
}

void tcp_connection::close() {
    if (!socket_) return;

    if (!outgoing_.empty())
        sk_warn("output queue is NOT empty!");

    handler_->disable_all();
    handler_->invalidate();
    socket_.reset();
    fn_on_close_(shared_from_this());
}

void tcp_connection::on_read() {
    static char buf[65536]; // 64KB
    ssize_t nbytes = socket_->recv(buf, sizeof(buf));
    if (nbytes > 0) {
        incoming_.append(buf, nbytes);
        if (fn_on_read_)
            fn_on_read_(0, shared_from_this(), &incoming_);
    } else if (nbytes == 0) {
        handler_->disable_reading();
        if (fn_on_read_)
            fn_on_read_(EOF, shared_from_this(), &incoming_);
    } else {
        int error = errno;
        sk_error("recv error: %s.", strerror(error));
        if (error != EAGAIN && error != EWOULDBLOCK) {
            handler_->disable_reading();
            if (fn_on_read_)
                fn_on_read_(error, shared_from_this(), nullptr);
        }
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
                fn_on_write_(0, shared_from_this());
        }
    } else {
        int error = errno;
        sk_error("send error: %s.", strerror(error));
        if (error != EAGAIN && error != EWOULDBLOCK) {
            handler_->disable_writing();
            if (fn_on_write_)
                fn_on_write_(error, shared_from_this());
        }
    }
}

NS_END(net)
NS_END(sk)
