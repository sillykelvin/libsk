#include "tcp_client.h"
#include "net/reactor.h"

NS_BEGIN(sk)

tcp_client_ptr tcp_client::create(reactor *r, const std::string& host,
                                  u16 port, const fn_on_connection& fn) {
    auto sock = socket::create();
    if (!sock) return nullptr;

    auto ptr = tcp_client_ptr(new tcp_client(r, host, port, fn));
    if (!ptr) return nullptr;

    ptr->socket_ = sock;
    return ptr;
}

int tcp_client::connect() {
    if (state_ != state_disconnected) {
        sk_warn("wrong state: %d.", state_);
        return 0;
    }

    int ret = socket_->connect(addr_);
    if (ret != 0) {
        sk_error("connect to %s error: %s.",
                 addr_.to_string().c_str(), strerror(errno));
        return ret;
    }

    state_ = state_connecting;
    reactor_->enable_writing(shared_from_this());
    return 0;
}

void tcp_client::on_event(int event) {
    // TODO: what should we do for the two events?
    if (event & reactor::EVENT_EPOLLERR)
        sk_error("epollerr");
    if (event & reactor::EVENT_EPOLLHUP)
        sk_error("epollhup");

    sk_assert(event & reactor::EVENT_WRITABLE);
    sk_assert(state_ == state_connecting);

    reactor_->disable_writing(shared_from_this());
    connection_ = connection::create(reactor_, socket_, addr_);
    if (!connection_) {
        sk_fatal("cannot create connection.");
        return;
    }

    state_ = state_connected;
    socket_.reset();
    fn_on_connection_(connection_);
}

NS_END(sk)
