#include "connector.h"
#include "log/log.h"
#include "net/reactor.h"
#include "utility/assert_helper.h"

NS_BEGIN(sk)

connector_ptr connector::create(reactor *r, const fn_on_connection& fn) {
    auto sock = socket::create();
    if (!sock) return nullptr;

    auto ptr = connector_ptr(new connector(r, fn));
    if (!ptr) return nullptr;

    ptr->socket_ = sock;
    return ptr;
}

int connector::connect(const inet_address& addr) {
    int ret = socket_->connect(addr);
    if (ret != 0) {
        sk_error("connect to <%s> error<%s>.",
                 addr.to_string().c_str(), strerror(errno));
        return ret;
    }

    ret = reactor_->register_handler(shared_from_this(), reactor::EVENT_WRITABLE);
    if (ret != 0) {
        sk_error("register error<%s>.", strerror(errno));
        return ret;
    }

    return 0;
}

void connector::on_event(int event) {
    // TODO: what should we do for the two events?
    if (event & reactor::EVENT_EPOLLERR)
        sk_error("epollerr");
    if (event & reactor::EVENT_EPOLLHUP)
        sk_error("epollhup");

    sk_assert(event & reactor::EVENT_WRITABLE);

    int ret = reactor_->deregister_handler(shared_from_this(), reactor::EVENT_WRITABLE);
    if (ret != 0) sk_error("deregister error<%s>.", strerror(errno));

    if (fn_on_connection_)
        fn_on_connection_(socket_);
}

NS_END(sk)
