#include "acceptor.h"

NS_BEGIN(sk)

handler_ptr acceptor::create(reactor *r) {
    auto sock = socket::create();
    if (!sock) return nullptr;

    auto ptr = std::shared_ptr<acceptor>(new acceptor(r));
    if (!ptr) return nullptr;

    ptr->socket_ = sock;
    return ptr;
}

int acceptor::listen(const std::string& addr, u16 port, int backlog) {
    return socket_->listen(addr, port, backlog);
}

void acceptor::on_event(int event) {
    // TODO
}

NS_END(sk)
