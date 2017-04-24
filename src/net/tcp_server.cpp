#include "tcp_server.h"
#include "net/reactor.h"
#include "log/log.h"

NS_BEGIN(sk)

tcp_server_ptr tcp_server::create(reactor *r, int backlog, u16 port,
                                  const fn_on_connection_event& fn_on_connection) {
    return tcp_server_ptr(new tcp_server(r, backlog, port, fn_on_connection));
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
        sk_warn("cannot find connection: %s.",
                conn->remote_address().to_string().c_str());
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

    auto conn = tcp_connection::create(reactor_, client, addr);
    conn->set_message_callback(fn_on_message_);
    conn->set_write_callback(fn_on_write_);
    conn->set_close_callback(std::bind(&tcp_server::remove_connection,
                                       this, std::placeholders::_1));

    connections_.insert(conn);
    fn_on_connection_(conn);
}

NS_END(sk)
