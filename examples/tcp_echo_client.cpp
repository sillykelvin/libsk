#include <iostream>
#include "net/tcp_client.h"
#include "net/reactor_epoll.h"

using namespace std;
using namespace sk::net;

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

    tcp_client client(r, "127.0.0.1", 8888,
                      [] (int error, const tcp_connection_ptr& conn) {
        if (error != 0) {
            sk_error("cannot connect: %s", strerror(error));
            return;
        }

        sk_debug("connected to 127.0.0.1:8888");
        conn->send("abc", 3);
    });

    client.on_read_event([] (int error, const tcp_connection_ptr& conn, buffer *buf) {
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
        buf->consume(buf->size());
        sk_debug("received: %s", str.c_str());

        if (str == "abc") {
            sk_debug("sleeping(3s)...");
            cout << "sleep" << endl;
            sleep(3);
            conn->send("def", 3);
        }
        else if (str == "def") {
            conn->send("quit", 4);
        } else  {
            conn->close();
        }
    });

    client.on_write_event([] (int error, const tcp_connection_ptr& conn) {
        if (error != 0) {
            sk_error("cannot write: %s", strerror(error));
            conn->close();
            return;
        }

        sk_debug("data written");
        conn->recv();
    });

    ret = client.connect();
    if (ret != 0) {
        cout << "fuck 4" << endl;
        return -4;
    }

    while (r->has_pending_event())
        r->dispatch(-1);

    cout << "exit" << endl;
    return 0;
}
