#include "tcp_server.h"
#include "net/reactor.h"
#include "log/log.h"

NS_BEGIN(sk)
NS_BEGIN(net)

tcp_server::tcp_server(reactor *r, int backlog, u16 port, const fn_on_connection& fn)
    : reactor_(r), backlog_(backlog), addr_(port),
      socket_(socket::create()), fn_on_connection_(fn),
      handler_(new handler(r, socket_->fd())) {
    handler_->on_read_event(std::bind(&tcp_server::on_accept, this));
}

tcp_server::~tcp_server() {
    sk_trace("~tcp_server(%s, %d, %lu)",
             addr_.to_string().c_str(), socket_->fd(), connections_.size());
}

int tcp_server::start() {
    int ret = socket_->listen(addr_, backlog_);
    if (ret != 0) {
        sk_error("listen on %s error: %s.",
                 addr_.to_string().c_str(), strerror(errno));
        return ret;
    }

    handler_->enable_reading();
    return 0;
}

void tcp_server::remove_connection(const tcp_connection_ptr& conn) {
    auto it = connections_.find(conn);
    if (it == connections_.end()) {
        sk_warn("cannot find connection: %s.", conn->name().c_str());
        return;
    }

    connections_.erase(it);
}

void tcp_server::on_accept() {
    inet_address addr(0);
    auto client = socket_->accept(addr);
    if (!client) {
        sk_error("accept error: %s.", strerror(errno));
        return;
    }

    tcp_connection_ptr conn(new tcp_connection(reactor_, client, addr,
                                               std::bind(&tcp_server::remove_connection,
                                                         this, std::placeholders::_1)));

    conn->on_read_event(fn_on_read_);
    conn->on_write_event(fn_on_write_);

    connections_.insert(conn);
    fn_on_connection_(0, conn);
}

NS_END(net)
NS_END(sk)
