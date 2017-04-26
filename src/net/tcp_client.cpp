#include "tcp_client.h"
#include "net/reactor.h"
#include "log/log.h"

NS_BEGIN(sk)
NS_BEGIN(net)

tcp_client::tcp_client(reactor *r, const std::string& host,
                       u16 port, const fn_on_connection& fn)
    : state_(state_disconnected),
      reactor_(r), addr_(host, port),
      socket_(socket::create()), fn_on_connection_(fn),
      handler_(new detail::handler(r, socket_->fd())) {
    handler_->on_write_event(std::bind(&tcp_client::on_connect, this));
}

tcp_client::~tcp_client() {
    sk_trace("~tcp_client(%s, %d, %s)",
             addr_.to_string().c_str(), socket_ ? socket_->fd() : -1,
             connection_ ? connection_->name().c_str() : "not connected");
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
    handler_->enable_writing();
    return 0;
}

void tcp_client::remove_connection(const tcp_connection_ptr& conn) {
    sk_assert(conn == connection_);

    state_ = state_disconnected;
    connection_.reset();
}

void tcp_client::on_connect() {
    sk_assert(state_ == state_connecting);

    int error = socket::get_error(socket_->fd());
    if (error != 0) {
        state_ = state_disconnected;
        handler_->disable_all();
        return fn_on_connection_(error, nullptr);
    }

    state_ = state_connected;
    handler_->disable_all();
    handler_.reset();

    connection_ = tcp_connection_ptr(new tcp_connection(reactor_, socket_, addr_,
                                                        std::bind(&tcp_client::remove_connection,
                                                                  this, std::placeholders::_1)));

    connection_->on_read_event(fn_on_read_);
    connection_->on_write_event(fn_on_write_);

    socket_.reset();
    fn_on_connection_(0, connection_);
}

NS_END(net)
NS_END(sk)
