#ifndef BUS_ROUTER_H
#define BUS_ROUTER_H

#include <map>
#include <set>
#include <list>
#include <string>
#include <net/tcp_client.h>
#include <net/tcp_server.h>
#include <net/tcp_connection.h>

struct bus_config;
struct bus_message;
class  bus_endpoint;
struct signalfd_siginfo;

namespace sk { class signal_watcher; class consul_client;
namespace detail { struct channel_mgr; }}

class bus_router {
public:
    int  init(const bus_config& cfg, bool resume_mode);
    int  stop();
    void fini();
    void reload(const bus_config& cfg);
    int  run();

private:
    class endpoint {
    public:
        endpoint(bus_router *r, const std::string& host, u16 port);
        int init();
        void stop();

        const std::string& host() const { return host_; }
        void set_connection(const sk::net::tcp_connection_ptr& conn) {
            this->connection_ = conn;
        }

        bool connected() const {
            bool yes = !!connection_;
            if (yes) sk_assert(!client_.connecting() && !client_.disconnected());
            return yes;
        }

        int send(const bus_message *msg);

    private:
        std::string host_;
        sk::net::tcp_client client_;
        sk::net::tcp_connection_ptr connection_;
    };

private:
    void report() const;
    int  handle_message(const bus_message *msg);
    int  send_local_message(const bus_message *msg);
    const std::string *find_host(int busid) const;
    void enqueue(int busid, const bus_message *msg);
    void enqueue(const std::string& host, const bus_message *msg);
    endpoint *fetch_endpoint(const std::string& host);

private:
    // tcp server callbacks
    void on_new_connection(int error, const sk::net::tcp_connection_ptr& conn);
    void on_remote_message_received(int error,
                                    const sk::net::tcp_connection_ptr& conn,
                                    sk::net::buffer *buf);
    void on_remote_message_sent(int error, const sk::net::tcp_connection_ptr& conn);

    // tcp client callbacks
    void on_remote_connected(endpoint *p, int error,
                             const sk::net::tcp_connection_ptr& conn);
    void on_local_message_received(endpoint *p, int error,
                                   const sk::net::tcp_connection_ptr& conn,
                                   sk::net::buffer *buf);
    void on_local_message_sent(endpoint *p,
                               int error, const sk::net::tcp_connection_ptr& conn);

    // signal callbacks
    void on_signal(const signalfd_siginfo *info);
    void on_local_message(int fd);
    void on_descriptor_change(int fd);

    // consul callbacks
    void on_route_watch(int ret, int index,
                        const std::map<std::string, std::string> *kv_list);
    void on_route_set(int ret, const std::string& key, const std::string& value);
    void on_route_del(int ret, bool recursive, const std::string& key);

private:
    static const int MAX_BACKLOG = 512;

    u16 listen_port_;
    int loop_rate_;          // how many messages will be processed in one loop
    size_t report_interval_;
    size_t running_count_;   // how many loops has been run

    bus_message *msg_;
    size_t buffer_capacity_;
    sk::detail::channel_mgr *mgr_;

    sk::net::reactor *reactor_;
    sk::consul_client *consul_;
    sk::net::tcp_server *server_;
    sk::signal_watcher *sig_watcher_;

    std::string localhost_; // ip of local host
    std::map<int, std::string> active_endpoints_;
    std::set<int> inactive_endpoints_;

    std::map<std::string, endpoint*> host2endpoints_;

    /*
     * messages whose destination does not exist in
     * busid2host_ will be stored here temporarily,
     * will be sent later when destination is fetched
     * from the consul service
     */
    using message_queue = std::list<bus_message*>;
    std::map<int, message_queue> busid2queue_;
    std::map<std::string, message_queue> host2queue_;
};

#endif // BUS_ROUTER_H
