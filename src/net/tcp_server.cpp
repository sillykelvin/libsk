#include "tcp_server.h"
#include "net/reactor.h"

NS_BEGIN(sk)

tcp_server_ptr tcp_server::create(reactor *r, int backlog,
                                  u16 port, const fn_on_connection& fn) {
    auto sock = socket::create();
    if (!sock) return nullptr;

    auto ptr = std::make_shared<tcp_server>(r, backlog, port, fn);
    if (!ptr) return nullptr;

    ptr->socket_ = sock;
    return ptr;
}

int tcp_server::start() {
    int ret = socket_->listen(addr_, backlog_);
    if (ret != 0) {
        sk_error("listen on %s error: %s.",
                 addr_.to_string().c_str(), strerror(errno));
        return ret;
    }

    reactor_->enable_reading(shared_from_this());
    return 0;
}

void tcp_server::remove_connection(const connection_ptr& conn) {
    auto it = connections_.find(conn);
    if (it == connections_.end()) {
        sk_warn("cannot find connection: %s.",
                conn->remote_address().to_string().c_str());
        return;
    }

    connections_.erase(it);
}

void tcp_server::on_event(int event) {
    // TODO: what should we do for the two events?
    if (event & reactor::EVENT_EPOLLERR)
        sk_error("epollerr");
    if (event & reactor::EVENT_EPOLLHUP)
        sk_error("epollhup");

    sk_assert(event & reactor::EVENT_READABLE);

    inet_address addr(0);
    auto client = socket_->accept(addr);
    if (!client) {
        sk_error("accept error: %s.", strerror(errno));
        return;
    }

    auto conn = connection::create(reactor_, client, addr);
    if (!conn) {
        sk_fatal("cannot create connection.");
        return;
    }

    connections_.insert(conn);
    fn_on_connection_(conn);
}

NS_END(sk)
