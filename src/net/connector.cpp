#include "connector.h"

NS_BEGIN(sk)

connector_ptr connector::create(reactor *r) {
    auto sock = socket::create();
    if (!sock) return nullptr;

    auto ptr = connector_ptr(new connector(r));
    if (!ptr) return nullptr;

    ptr->socket_ = sock;
    return ptr;
}

int connector::connect(const inet_address& addr) {
    return socket_->connect(addr);
}

void connector::on_event(int event) {
    // TODO
}

NS_END(sk)
