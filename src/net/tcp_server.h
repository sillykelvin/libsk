#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <set>
#include "net/connection.h"

NS_BEGIN(sk)

class tcp_server;
typedef std::shared_ptr<tcp_server> tcp_server_ptr;

class tcp_server : public handler {
public:
    MAKE_NONCOPYABLE(tcp_server);
    typedef std::function<void(const connection_ptr&)> fn_on_connection;

    static tcp_server_ptr create(reactor *r, int backlog,
                                 u16 port, const fn_on_connection& fn);
    // TODO: should we destroy connections_ manually in destructor?
    virtual ~tcp_server() = default;

    int start();
    void remove_connection(const connection_ptr& conn);

    virtual void on_event(int event) override;
    virtual int handle() const override { return socket_->fd(); }

private:
    tcp_server(reactor *r, int backlog, u16 port, const fn_on_connection& fn)
        : handler(r), backlog_(backlog), addr_(port), fn_on_connection_(fn) {}

private:
    int backlog_;
    inet_address addr_;
    socket_ptr socket_;
    fn_on_connection fn_on_connection_;
    std::set<connection_ptr> connections_;
};

NS_END(sk)

#endif // TCP_SERVER_H
