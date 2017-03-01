#ifndef BUS_ROUTER_H
#define BUS_ROUTER_H

#include "consul.h"
#include "bus/detail/channel_mgr.h"

struct bus_config;
struct bus_message;

class bus_router {
public:
    int init(const bus_config& cfg, bool resume_mode);
    void fini();
    void reload(const bus_config& cfg);

    int run();

private:
    void report() const;

    bool update_route(); // returns false if idle, true if busy, the same below
    bool run_agent();
    bool fetch_msg();
    bool process_msg();

private:
    bool is_local_process(int busid) const {
        return local_procs_.find(busid) != local_procs_.end();
    }

    const std::string *find_host(int busid) const {
        auto it = busid2host_.find(busid);
        if (it == busid2host_.end())
            return nullptr;

        return &(it->second);
    }

    int *fetch_socket(const std::string& host);

private:
    void on_route_get_all(int ret, const consul::string_map *kv_list);
    void on_route_set(int ret, const std::string& key, const std::string& value);
    void on_route_del(int ret, bool recursive, const std::string& key);

private:
    bus_message *msg_;
    size_t buffer_capacity_;
    sk::detail::channel_mgr *mgr_;

    int recv_port_; // which port the receiver is work on
    int receiver_;  // nanomsg receiver socket
    std::map<std::string, int> host2sender_; // nanomsg sender sockets

    consul agent_;
    size_t pending_count_; // pending requests count

    size_t update_count_;     // how many times update has been executed
    time_t last_update_time_; // last time update has been executed

    int loop_rate_;          // how many messages will be processed in one loop
    size_t report_interval_;
    size_t running_count_;   // how many loops has been run

    std::string local_host_;    // ip of local host
    std::set<int> local_procs_; // registered busid of processes running on local host
    std::map<int, std::string> busid2host_;
};

#endif // BUS_ROUTER_H
