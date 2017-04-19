#include <iostream>
#include "net/tcp_server.h"
#include "net/reactor_epoll.h"

using namespace sk;
using namespace std;

int main() {
    reactor *r = reactor_epoll::create();
    if (!r) {
        cout << "fuck" << endl;
        return -1;
    }

    auto server = tcp_server::create(r, 32, 8888,
                                     [](const connection_ptr& conn) {
        conn->set_read_callback([](const connection_ptr& conn, buffer *buf) {
            std::string str(buf->peek(), buf->size());
            cout << "received: " << str << endl;

            conn->send(buf->peek(), buf->size());
            buf->consume(buf->size());
        });

        conn->set_write_callback([](const connection_ptr& conn) {
            cout << "data written back" << endl;
        });

        conn->set_close_callback([](const connection_ptr& conn) {
            cout << "connection closed" << endl;
        });

        conn->recv();
    });

    if (!server) {
        cout << "fuck 2" << endl;
        return -2;
    }

    int ret = server->start();
    if (ret != 0) {
        cout << "fuck 3" << endl;
        return -3;
    }

    while (1) r->dispatch(-1);
    cout << "exit" << endl;
    return 0;
}
