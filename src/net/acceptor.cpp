#include "acceptor.h"
#include "log/log.h"
#include "net/reactor.h"
#include "net/connection.h"
#include "utility/assert_helper.h"

NS_BEGIN(sk)

acceptor_ptr acceptor::create(reactor *r, const fn_on_connection& fn) {
    auto sock = socket::create();
    if (!sock) return nullptr;

    auto ptr = acceptor_ptr(new acceptor(r, fn));
    if (!ptr) return nullptr;

    ptr->socket_ = sock;
    return ptr;
}

int acceptor::listen(const inet_address& addr, int backlog) {
    int ret = socket_->listen(addr, backlog);
    if (ret != 0) {
        sk_error("listen on <%s> error<%s>.",
                 addr.to_string().c_str(), strerror(errno));
        return ret;
    }

    ret = reactor_->register_handler(shared_from_this(), reactor::EVENT_READABLE);
    if (ret != 0) {
        sk_error("register error<%s>.", strerror(errno));
        return ret;
    }

    return 0;
}

void acceptor::on_event(int event) {
    // we do nothing here for the two events
    if (event & reactor::EVENT_EPOLLERR)
        sk_error("epollerr");
    if (event & reactor::EVENT_EPOLLHUP)
        sk_error("epollhup");

    sk_assert(event & reactor::EVENT_READABLE);

    inet_address addr(0);
    auto client = socket_->accept(addr);
    if (!client) {
        sk_error("accept error<%s>.", strerror(errno));
        return;
    }

    if (fn_on_connection_)
        fn_on_connection_(client, addr);
}

NS_END(sk)
