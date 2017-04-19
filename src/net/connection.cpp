#include "connection.h"
#include "net/reactor.h"
#include "utility/assert_helper.h"

NS_BEGIN(sk)

connection_ptr connection::create(reactor *r,
                                  const socket_ptr& socket,
                                  const inet_address& remote_addr) {
    sk_assert(socket);
    return connection_ptr(new connection(r, socket, remote_addr));
}

ssize_t connection::send(const void *data, size_t len) {
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
                fn_on_write_(std::static_pointer_cast<connection>(shared_from_this()));
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

    auto self(shared_from_this());
    if (!reactor_->writing_enabled(self))
        reactor_->enable_writing(self);

    return nbytes;
}

int connection::recv() {
    if (!socket_) {
        sk_error("invalid socket.");
        return -1;
    }

    auto self(shared_from_this());
    if (!reactor_->reading_enabled(self))
        reactor_->enable_reading(self);

    return 0;
}

void connection::close() {
    if (!socket_) return;

    if (!outgoing_.empty())
        sk_warn("output queue is NOT empty!");

    auto self(shared_from_this());
    reactor_->disable_all(self);

    // TODO: better handling here than just forcly close the socket fd
    socket_.reset();

    if (fn_on_close_)
        fn_on_close_(std::static_pointer_cast<connection>(self));
}

void connection::on_event(int event) {
    // TODO: what should we do for the two events?
    if (event & reactor::EVENT_EPOLLERR)
        sk_error("epollerr");
    if (event & reactor::EVENT_EPOLLHUP)
        sk_error("epollhup");

    if (event & reactor::EVENT_WRITABLE) {
        sk_assert(!outgoing_.empty());
        ssize_t nbytes = socket_->send(outgoing_.peek(), outgoing_.size());
        if (nbytes >= 0) {
            outgoing_.consume(nbytes);
            if (outgoing_.empty()) {
                auto self(shared_from_this());
                reactor_->disable_writing(self);

                if (fn_on_write_)
                    fn_on_write_(std::static_pointer_cast<connection>(self));
            }
        } else {
            // TODO: disable writing here?
            sk_error("send error<%s>.", strerror(errno));
        }
    }

    if (event & reactor::EVENT_READABLE) {
        static char buf[65536]; // 64KB
        ssize_t nbytes = socket_->recv(buf, sizeof(buf));
        if (nbytes > 0) {
            incoming_.append(buf, nbytes);
            if (fn_on_read_) {
                auto self(shared_from_this());
                fn_on_read_(std::static_pointer_cast<connection>(self), &incoming_);
            }
        } else if (nbytes == 0) {
            // TODO: a better handler here
            reactor_->disable_reading(shared_from_this());
            close();
        } else {
            // TODO: disable reading here?
            sk_error("recv error<%s>.", strerror(errno));
        }
    }
}

NS_END(sk)
