#include <iostream>
#include <libsk.h>

using namespace sk;
using namespace std;
using namespace net;
using namespace std::placeholders;

void error_exit(const char *error) {
    cout << error << endl;
    exit(-1);
}

void on_watch(consul_client *client, int ret, int index, const map<string, string> *kv) {
    cout << "watch returns: " << ret << endl;
    cout << "consul index: " << index << endl;
    if (kv) {
        for (const auto& it : *kv)
            cout << it.first << " -> " << it.second << endl;
    }

    client->watch("bus/", index, bind(on_watch, client, _1, _2, _3));
}

void on_del(consul_client *client, int ret, bool recursive, const string& key) {
    (void) client;
    cout << "del returns: " << ret << endl;
    cout << "del key: " << key << ", recursive: " << std::boolalpha << recursive << endl;
}

void on_set(consul_client *client, int ret, const string& key, const string& value) {
    cout << "set returns: " << ret << endl;
    cout << "set key: " << key << ", value: " << value << endl;

    client->del(key, false, bind(on_del, client, _1, _2, _3));
}

int main() {
    int ret = logger::init("log.xml");
    if (ret != 0) error_exit("fuck 1");

    ret = time::init_time(0);
    if (ret != 0) error_exit("fuck 2");

    ret = time::init_heap_timer();
    if (ret != 0) error_exit("fuck 3");

    reactor *r = reactor_epoll::create();
    if (!r) error_exit("fuck 4");

    vector<string> addr_list;
    addr_list.push_back("127.0.0.1:8500");

    consul_client *client = new consul_client();
    ret = client->init(r, addr_list);
    if (ret != 0) error_exit("fuck 5");

    client->watch("bus/", 0, bind(on_watch, client, _1, _2, _3));
    client->set("bus/test", "a test key", bind(on_set, client, _1, _2, _3));

    // TODO: improve the loop here
    while (true) {
        if (time::time_enabled(nullptr))
            time::update_time();

        r->dispatch(10);
    }

    return 0;
}
