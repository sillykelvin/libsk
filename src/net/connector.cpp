#include "connector.h"

NS_BEGIN(sk)

handler_ptr connector::create(reactor *r) {
    auto sock = socket::create();
    if (!sock) return nullptr;

    auto ptr = std::shared_ptr<connector>(new connector(r));
    if (!ptr) return nullptr;

    ptr->socket_ = sock;
    return ptr;
}

int connector::connect(const std::string& addr, u16 port) {
    return socket_->connect(addr, port);
}

void connector::on_event(int event) {
    // TODO
}

NS_END(sk)
