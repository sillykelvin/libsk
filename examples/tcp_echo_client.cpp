#include <iostream>
#include "net/tcp_client.h"
#include "net/reactor_epoll.h"

using namespace sk;
using namespace std;

int main() {
    int ret = sk::logger::init("client_log.xml");
    if (ret != 0) {
        cout << "fuck 1" << endl;
        return -1;
    }

    reactor *r = reactor_epoll::create();
    if (!r) {
        cout << "fuck 2" << endl;
        return -2;
    }

    auto client = tcp_client::create(r, "127.0.0.1", 8888,
                                     [](int error, const connection_ptr& conn) {
        if (error != 0) {
            cout << "error: " << strerror(error) << endl;
            return;
        }

        conn->set_read_callback([](const connection_ptr& conn, buffer *buf) {
            std::string str(buf->peek(), buf->size());
            buf->consume(buf->size());
            cout << "received: " << str << endl;

            if (str == "abc")
                conn->send("def", 3);
            else if (str == "def")
                conn->send("quit", 4);
            else
                conn->close();
        });

        conn->set_write_callback([](const connection_ptr& conn) {
            cout << "data written" << endl;
            conn->recv();
        });

        conn->set_close_callback([](const connection_ptr& conn) {
            cout << "connection closed" << endl;
        });

        conn->send("abc", 3);
    });

    if (!client) {
        cout << "fuck 3" << endl;
        return -3;
    }

    ret = client->connect();
    if (ret != 0) {
        cout << "fuck 4" << endl;
        return -4;
    }

    while (1) r->dispatch(-1);
    cout << "exit" << endl;
    return 0;
}
