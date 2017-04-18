#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include "net/connection.h"

NS_BEGIN(sk)

class tcp_client;
typedef std::shared_ptr<tcp_client> tcp_client_ptr;

class tcp_client : public handler {
public:
    MAKE_NONCOPYABLE(tcp_client);
    typedef std::function<void(const connection_ptr&)> fn_on_connection;

    static tcp_client_ptr create(reactor *r, const std::string& host,
                                 u16 port, const fn_on_connection& fn);
    // TODO: should we destroy connection_ manually in destructor?
    virtual ~tcp_client() = default;

    int connect();

    virtual void on_event(int event) override;
    virtual int handle() const override { return socket_->fd(); }

private:
    tcp_client(reactor *r, const std::string& host,
               u16 port, const fn_on_connection& fn)
        : handler(r), state_(state_disconnected),
          addr_(host, port), fn_on_connection_(fn) {}

private:
    enum state {
        state_connecting,
        state_connected,
        state_disconnected
    };

    state state_;
    inet_address addr_;
    socket_ptr socket_;
    connection_ptr connection_;
    fn_on_connection fn_on_connection_;
};

NS_END(sk)

#endif // TCP_CLIENT_H
