#include "tcp_client.h"
#include "net/reactor.h"
#include "log/log.h"

NS_BEGIN(sk)

tcp_client_ptr tcp_client::create(reactor *r, const std::string& host, u16 port,
                                  const fn_on_connection_event& fn_on_connection,
                                  const fn_on_error_event& fn_on_error) {
    return tcp_client_ptr(new tcp_client(r, host, port, fn_on_connection, fn_on_error));
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

    connection_.reset();
}

void tcp_client::on_connect() {
    sk_assert(state_ == state_connecting);
    state_ = state_connected;

    handler_->disable_all();
    handler_.reset();

    connection_ = tcp_connection::create(reactor_, socket_, addr_);
    connection_->set_message_callback(fn_on_message_);
    connection_->set_write_callback(fn_on_write_);
    connection_->set_close_callback(std::bind(&tcp_client::remove_connection,
                                              this, std::placeholders::_1));

    socket_.reset();
    fn_on_connection_(connection_);
}

void tcp_client::on_error() {
    sk_assert(state_ == state_connecting);

    int error = socket::get_error(socket_->fd());
    sk_error("connect to %s error: %s.",
             addr_.to_string().c_str(), strerror(error));

    state_ = state_disconnected;
    handler_->disable_writing();
    fn_on_error_(error);
}

NS_END(sk)
