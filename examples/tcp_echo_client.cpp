#include <iostream>
#include "net/tcp_client.h"
#include "net/reactor_epoll.h"

using namespace sk;
using namespace std;

int main() {
    reactor *r = reactor_epoll::create();
    if (!r) {
        cout << "fuck" << endl;
        return -1;
    }

    auto client = tcp_client::create(r, "127.0.0.1", 8888,
                                     [](const connection_ptr& conn) {
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
        cout << "fuck 2" << endl;
        return -2;
    }

    int ret = client->connect();
    if (ret != 0) {
        cout << "fuck 3" << endl;
        return -3;
    }

    while (1) r->dispatch(-1);
    cout << "exit" << endl;
    return 0;
}
