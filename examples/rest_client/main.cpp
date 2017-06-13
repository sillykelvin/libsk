#include <iostream>
#include <libsk.h>

using namespace sk;
using namespace std;
using namespace net;

void error_exit(const char *error) {
    cout << error << endl;
    exit(-1);
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

    rest_client *client = new rest_client(r, "bing.com", 1);
    if (!client) error_exit("fuck 5");

    client->get(nullptr,
                [&] (int ret,
                    int status,
                    const rest_client::string_map& headers,
                    const std::string& body) {
        cout << "------ uri: / ------" << endl;
        cout << "ret: " << ret << endl;
        cout << "status: " << status << endl;
        cout << "headers: " << endl;
        for (const auto& it : headers)
            cout << it.first << " -> " << it.second << endl;
        cout << "body: " << body << endl;
        cout << "------ uri: / ------" << endl;
        return 0;
    }, nullptr, nullptr);

    client->get("/query",
                [&client] (int ret,
                           int status,
                           const rest_client::string_map& headers,
                           const std::string& body) {
        cout << "------ uri: /query ------" << endl;
        cout << "ret: " << ret << endl;
        cout << "status: " << status << endl;
        cout << "headers: " << endl;
        for (const auto& it : headers)
            cout << it.first << " -> " << it.second << endl;
        cout << "body: " << body << endl;
        cout << "------ uri: /query ------" << endl;

        client->get("/sequential_query",
                    [] (int ret,
                        int status,
                        const rest_client::string_map& headers,
                        const std::string& body) {
            cout << "------ uri: /sequential_query ------" << endl;
            cout << "this response reuses previous connection" << endl;
            cout << "ret: " << ret << endl;
            cout << "status: " << status << endl;
            cout << "headers: " << endl;
            for (const auto& it : headers)
                cout << it.first << " -> " << it.second << endl;
            cout << "body: " << body << endl;
            cout << "------ uri: /sequential_query ------" << endl;
        }, nullptr, nullptr);
    }, nullptr, nullptr);

    // TODO: improve the loop here
    while (true) {
        if (time::time_enabled(nullptr))
            time::update_time();

        r->dispatch(10);
    }

    return 0;
}
