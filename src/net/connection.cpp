#include "connection.h"
#include "net/reactor.h"
#include "utility/assert_helper.h"

NS_BEGIN(sk)

connection_ptr connection::create(reactor *r,
                                  const socket_ptr& socket,
                                  const inet_address& local_addr,
                                  const inet_address& remote_addr) {
    return std::make_shared<connection>(r, socket, local_addr, remote_addr);
}

connection::connection(reactor *r,
                       const socket_ptr& socket,
                       const inet_address& local_addr,
                       const inet_address& remote_addr)
    : state_(state_disconnected),
      handler(r),
      socket_(socket),
      local_addr_(local_addr),
      remote_addr_(remote_addr)
{}

ssize_t connection::send(const void *data, size_t len) {
    if (state_ != state_connected) {
        sk_error("wrong state<%d>.", state_);
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
    if (!reactor_->event_registered(self, reactor::EVENT_WRITABLE))
        reactor_->register_handler(self, reactor::EVENT_WRITABLE);

    return nbytes;
}

int connection::recv() {
    if (state_ != state_connected) {
        sk_error("wrong state<%d>.", state_);
        return -1;
    }

    auto self(shared_from_this());
    if (!reactor_->event_registered(self, reactor::EVENT_READABLE))
        reactor_->register_handler(self, reactor::EVENT_READABLE);

    return 0;
}

void connection::close() {
    if (state_ == state_disconnected) return;

    sk_debug("current state<%d>.", state_);
    state_ = state_disconnected;

    auto self(shared_from_this());
    reactor_->deregister_handler(self, reactor::EVENT_READABLE | reactor::EVENT_WRITABLE);

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
                reactor_->deregister_handler(self, reactor::EVENT_WRITABLE);

                if (fn_on_write_)
                    fn_on_write_(std::static_pointer_cast<connection>(self));
            }
        } else {
            sk_error("send error<%s>.", strerror(errno));
        }
    }

    if (event & reactor::EVENT_READABLE) {
        static char buf[8192];
        ssize_t nbytes = socket_->recv(buf, sizeof(buf));
        if (nbytes > 0) {
            if (fn_on_message_) {
                auto self(shared_from_this());
                fn_on_message_(std::static_pointer_cast<connection>(self), &incoming_);
            }
        } else if (nbytes == 0) {
            close();
        } else {
            sk_error("recv error<%s>.", strerror(errno));
        }
    }
}

NS_END(sk)
