#include "connection.h"
#include "utility/assert_helper.h"

NS_BEGIN(sk)

handler_ptr connection::create(reactor *r) {
    return std::shared_ptr<connection>(new connection(r));
}

void connection::set_socket(const socket_ptr& socket) {
    sk_assert(!socket_);
    socket_ = socket;
}

int connection::send(const void *data, size_t len) {
    // TODO
    return 0;
}

int connection::recv() {
    // TODO
    return 0;
}

void connection::on_event(int event) {
    // TODO
}

NS_END(sk)
