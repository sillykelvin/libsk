#include <iostream>
#include "net/tcp_server.h"
#include "net/reactor_epoll.h"

using namespace std;
using namespace sk::net;

int main() {
    int ret = sk::logger::init("server_log.xml");
    if (ret != 0) {
        cout << "fuck 1" << endl;
        return -1;
    }

    reactor *r = reactor_epoll::create();
    if (!r) {
        cout << "fuck 2" << endl;
        return -2;
    }

    tcp_server server(r, 32, 8888,
                      [] (int error, const tcp_connection_ptr& conn) {
        if (error != 0) {
            sk_error("cannot accept: %s", strerror(error));
            return;
        }

        sk_debug("client %s connected.", conn->remote_address().to_string().c_str());
        conn->recv();
    });

    server.on_read_event([] (int error, const tcp_connection_ptr& conn, buffer *buf) {
        if (error == EOF) {
            sk_debug("eof received.");
            conn->close();
            return;
        }

        if (error != 0) {
            sk_error("cannot read: %s", strerror(error));
            conn->close();
            return;
        }

        std::string str(buf->peek(), buf->size());
        sk_debug("received: %s", str.c_str());

        conn->send(buf->peek(), buf->size());
        buf->consume(buf->size());
    });

    server.on_write_event([] (int error, const tcp_connection_ptr& conn) {
        if (error != 0) {
            sk_error("cannot write: %s", strerror(error));
            conn->close();
            return;
        }

        sk_debug("data written back");
    });

    ret = server.start();
    if (ret != 0) {
        cout << "fuck 4" << endl;
        return -4;
    }

    while (r->has_pending_event())
        r->dispatch(-1);

    cout << "exit" << endl;
    return 0;
}
