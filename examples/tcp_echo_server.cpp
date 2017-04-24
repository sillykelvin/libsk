#include <iostream>
#include "net/tcp_server.h"
#include "net/reactor_epoll.h"

using namespace sk;
using namespace std;

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

    auto server = tcp_server::create(r, 32, 8888,
                                     [](const tcp_connection_ptr& conn) {
        conn->recv();
    });

    if (!server) {
        cout << "fuck 3" << endl;
        return -3;
    }

    server->set_message_callback([](const tcp_connection_ptr& conn, buffer *buf) {
        std::string str(buf->peek(), buf->size());
        cout << "received: " << str << endl;

        conn->send(buf->peek(), buf->size());
        buf->consume(buf->size());
    });

    server->set_write_callback([](const tcp_connection_ptr& conn) {
        cout << "data written back" << endl;
    });

    ret = server->start();
    if (ret != 0) {
        cout << "fuck 4" << endl;
        return -4;
    }

    while (r->has_event())
        r->dispatch(-1);
    cout << "exit" << endl;
    return 0;
}
