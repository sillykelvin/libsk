/************************************************
 *
 * TODO: the example is old, have to comment it
 * out to compile, may update new example later
 *
 ************************************************

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

    const int total_count = 100000;
    int recv_count = 0;

    tcp_client client(r, "127.0.0.1", 8888,
                      [] (int error, const tcp_connection_ptr& conn) {
        if (error != 0) {
            sk_error("cannot connect: %s", strerror(error));
            return;
        }

        sk_debug("connected to 127.0.0.1:8888");

        std::string str;
        for (int i = 0; i < total_count; ++i)
            str.append("a");
        conn->send(str.data(), str.length());
    });

    client.set_read_callback([&recv_count, &total_count] (int error, const tcp_connection_ptr& conn, buffer *buf) {
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
        if (str.length() >= 5)
            sk_debug("received length: %lu", str.length());
        else
            sk_debug("received: %s", str.c_str());

        if (str == "abc") {
            sk_debug("sleeping(3s)...");
            cout << "sleep" << endl;
            sleep(3);
            conn->send("def", 3);
        }
        else if (str == "def") {
            conn->send("quit", 4);
        } else if (str == "quit") {
            conn->close();
        } else {
            recv_count += str.length();
            if (recv_count >= total_count) {
                sk_debug("all received, send abc now");
                conn->send("abc", 3);
            }
        }
    });

    client.set_write_callback([] (int error, const tcp_connection_ptr& conn) {
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

************************************************/

int main() { return 0; }
