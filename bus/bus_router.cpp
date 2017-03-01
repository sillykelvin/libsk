#include <string.h>
#include <ifaddrs.h>
#include <sys/shm.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "bus/bus.h"
#include "bus_router.h"
#include "bus_config.h"
#include "nanomsg/nn.h"
#include "nanomsg/pair.h"
#include "utility/config.h"
#include "common/lock_guard.h"
#include "utility/string_helper.h"
#include "shm/detail/shm_segment.h"

#define BUS_KV_PREFIX "bus/"
#define UPDATE_INITIAL_INTERVAL  1  // update every 1 seconds at initial time
#define UPDATE_STEADY_INTERVAL   10 // update every 10 seconds at steady time
#define UPDATE_INITIAL_THRESHOLD 60 // consider the state "steady" after 60 updates

struct bus_message {
    int magic;
    int waste;     // for alignment
    int src_busid;
    int dst_busid;
    size_t length;
    char data[0];

    void init(size_t capacity) {
        this->magic = MAGIC;
        this->waste = 0;
        this->src_busid = 0;
        this->dst_busid = 0;
        this->length = capacity;
    }

    void reset(size_t capacity) {
        length = capacity;
    }

    size_t total_length() const {
        return sizeof(*this) + length;
    }
};
static_assert(std::is_pod<bus_message>::value, "bus_message must be a POD type.");

static int retrieve_local_address(std::string& ip) {
    ip.clear();

    ifaddrs *addr = nullptr;
    getifaddrs(&addr);
    for (ifaddrs *ifa = addr; ifa; ifa = ifa->ifa_next) {
        check_continue(ifa->ifa_addr);
        check_continue(ifa->ifa_addr->sa_family == AF_INET);
        check_continue(strcmp(ifa->ifa_name, "lo") != 0);

        void *tmp = &((sockaddr_in *) ifa->ifa_addr)->sin_addr;
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, tmp, buf, INET_ADDRSTRLEN);

        ip = buf;
        sk_info("ip of localhost: %s", ip.c_str());
    }

    if (ip.empty()) {
        sk_error("no valid network interface.");
        return -ENODEV;
    }

    return 0;
}

int bus_router::init(const bus_config& cfg, bool resume_mode) {
    int ret = 0;

    buffer_capacity_ = 2 * 1024 * 1024;
    msg_ = cast_ptr(bus_message, malloc(sizeof(bus_message) + buffer_capacity_));
    if (!msg_) return -ENOMEM;
    msg_->init(buffer_capacity_);

    sk::detail::shm_segment seg;
    ret = seg.init(cfg.bus_shm_key, cfg.bus_shm_size, resume_mode);
    if (ret != 0) return ret;

    mgr_ = cast_ptr(sk::detail::channel_mgr, seg.address());
    ret = mgr_->init(seg.shmid, cfg.bus_shm_size, resume_mode);
    if (ret != 0) return ret;

    int socket = nn_socket(AF_SP, NN_PAIR);
    if (socket < 0) return -1;

    char addr[128] = {0};
    snprintf(addr, sizeof(addr) - 1, "tcp://*:%d", cfg.listen_port);
    int endpoint = nn_bind(socket, addr);
    if (endpoint < 0) return -1;

    ret = agent_.init(cfg.consul_addr_list);
    if (ret != 0) return ret;

    ret = retrieve_local_address(local_host_);
    if (ret != 0) return ret;

    recv_port_ = cfg.listen_port;
    receiver_ = socket;

    pending_count_ = 0;

    update_count_ = 0;
    last_update_time_ = 0;

    loop_rate_ = (cfg.msg_per_run != 0) ? cfg.msg_per_run : 200;
    report_interval_ = cfg.report_interval;
    running_count_ = 0;

    seg.release();
    return 0;
}

void bus_router::fini() {
    if (msg_) {
        free(msg_);
        msg_ = nullptr;
        buffer_capacity_ = 0;
    }

    if (mgr_) {
        shmctl(mgr_->shmid, IPC_RMID, 0);
        mgr_ = nullptr;
    }
}

void bus_router::reload(const bus_config& cfg) {
    loop_rate_ = cfg.msg_per_run;
    report_interval_ = cfg.report_interval;
}

int bus_router::run() {
    sk::lock_guard<sk::spin_lock> guard(mgr_->lock);

    ++running_count_;
    report();

    bool ubusy = update_route();
    bool rbusy = run_agent();
    bool fbusy = fetch_msg();
    bool pbusy = process_msg();

    // return 1 to make the main loop busy running
    return (ubusy || rbusy || fbusy || pbusy) ? 1 : 0;
}

void bus_router::report() const {
    // interval == 0 means reporting is disabled
    if (report_interval_ == 0)
        return;

    if (running_count_ % report_interval_ != 0)
        return;

    std::string info("local processes: [ ");
    for (const auto& it : local_procs_)
        info.append(sk::bus::to_string(it)).append(" ");
    info.append("]");
    sk_info("%s", info.c_str());

    for (const auto& it : busid2host_) {
        std::string str_busid = sk::bus::to_string(it.first);
        sk_info("route map: %s -> %s", str_busid.c_str(), it.second.c_str());
    }

    mgr_->report();
}

bool bus_router::update_route() {
    using namespace std::placeholders;

    time_t now = time(nullptr);
    time_t interval = UPDATE_STEADY_INTERVAL;
    if (unlikely(update_count_ < UPDATE_INITIAL_THRESHOLD))
        interval = UPDATE_INITIAL_INTERVAL;

    if (!mgr_->changed && last_update_time_ + interval > now)
        return false;

    // reset the change, do NOT lock mgr_ here
    // as we have locked mgr in function run()
    // sk::lock_guard<sk::spin_lock> guard(mgr_->lock);
    mgr_->changed = false;

    for (int i = 0; i < mgr_->descriptor_count; ++i) {
        const sk::detail::channel_descriptor& desc = mgr_->descriptors[i];
        auto it = local_procs_.find(desc.owner);

        if (desc.closed  && it == local_procs_.end()) continue;
        if (!desc.closed && it != local_procs_.end()) continue;

        // channel registered, but not added to consul
        if (!desc.closed && it == local_procs_.end()) {
            std::string key(BUS_KV_PREFIX + sk::bus::to_string(desc.owner));
            agent_.set(key, local_host_, std::bind(&bus_router::on_route_set, this, _1, _2, _3));
            pending_count_ += 1;
            continue;
        }

        // channel closed, but not removed from consul
        if (desc.closed && it != local_procs_.end()) {
            std::string key(BUS_KV_PREFIX + sk::bus::to_string(desc.owner));
            agent_.del(key, false, std::bind(&bus_router::on_route_del, this, _1, _2, _3));
            pending_count_ += 1;
            continue;
        }

        sk_assert(0);
    }

    agent_.get_all(BUS_KV_PREFIX, std::bind(&bus_router::on_route_get_all, this, _1, _2));
    pending_count_ += 1;

    update_count_ += 1;
    last_update_time_ = now;
    return true;
}

bool bus_router::run_agent() {
    if (pending_count_ > 0)
        agent_.run();

    return pending_count_ > 0;
}

bool bus_router::fetch_msg() {
    void *buf = nullptr;
    int nbytes = nn_recv(receiver_, &buf, NN_MSG, NN_DONTWAIT);
    sk_assert(nbytes > 0 || nbytes == -1);

    if (nbytes == -1) {
        int error = nn_errno();
        // EAGAIN means there is no message to fetch
        if (error != EAGAIN)
            sk_error("receive message error<%d:%s>.", error, nn_strerror(error));

        return false;
    }

    struct guard {
        void *msg;
        guard(void *msg) : msg(msg) {}
        ~guard() { if (msg) nn_freemsg(msg); }
    } g(buf);

    bus_message *msg = cast_ptr(bus_message, buf);
    sk_assert(msg->magic == MAGIC);
    sk_assert(msg->waste == 0);
    sk_assert(msg->src_busid != 0);
    sk_assert(msg->dst_busid != 0);
    sk_assert((size_t) nbytes == msg->total_length());

    // the following logic all returns true as there is already a message
    // fetched from the socket, we should assume there are more to fetch

    if (!is_local_process(msg->dst_busid)) {
        sk_error("process<%x> is not on this host, src<%x>.", msg->dst_busid, msg->src_busid);
        return true;
    }

    sk::detail::channel *rc = mgr_->find_read_channel(msg->dst_busid);
    if (!rc) {
        sk_error("cannot find channel<%x>.", msg->dst_busid);
        return true;
    }

    int ret = rc->push(msg->src_busid, msg->dst_busid, msg->data, msg->length);
    if (ret != 0)
        sk_error("push message error<%d>, bus<%x>.", ret, msg->dst_busid);

    return true;
}

bool bus_router::process_msg() {
    // no channel on this host
    if (mgr_->descriptor_count <= 0)
        return false;

    int count = 0;
    int index = 0;
    std::set<int> empty_channels;
    while (count < loop_rate_) {
        const sk::detail::channel_descriptor& desc = mgr_->descriptors[index];
        if (desc.closed) {
            empty_channels.insert(desc.owner);
            index = (index + 1) % mgr_->descriptor_count;

            if (empty_channels.size() < (size_t) mgr_->descriptor_count)
                continue;

            // all channels are empty
            return false;
        }

        sk::detail::channel *wc = mgr_->get_write_channel(index);
        index = (index + 1) % mgr_->descriptor_count;

        msg_->reset(buffer_capacity_);
        int ret = wc->pop(msg_->data, msg_->length, &msg_->src_busid, &msg_->dst_busid);

        // no message in this channel
        if (ret == 0) {
            empty_channels.insert(desc.owner);
            if (empty_channels.size() < (size_t) mgr_->descriptor_count)
                continue;

            // all channels are empty
            return false;
        }

        if (unlikely(ret < 0)) {
            if (ret != -E2BIG) {
                sk_error("pop message error, ret<%d>, process<%d>.", ret, desc.owner);
                continue;
            }

            sk_warn("big message, size<%lu>, buffer size<%d>.", msg_->length, buffer_capacity_);
            bus_message *buf = cast_ptr(bus_message, malloc(msg_->total_length()));
            assert_continue(buf);

            buffer_capacity_ = msg_->length;
            buf->init(buffer_capacity_);
            free(msg_);
            msg_ = buf;

            ret = wc->pop(msg_->data, msg_->length, &msg_->src_busid, &msg_->dst_busid);
            assert_continue(ret != 0);

            if (ret < 0) {
                sk_assert(ret != -E2BIG);
                sk_error("pop message error, ret<%d>, process<%d>.", ret, desc.owner);
                continue;
            }
        }

        if (likely(ret == 1)) {
            count += 1;

            auto it = empty_channels.find(desc.owner);
            if (it != empty_channels.end())
                empty_channels.erase(it);

            if (msg_->src_busid != desc.owner)
                sk_warn("bus mismatch, message<%x>, channel<%x>.", msg_->src_busid, desc.owner);

            // dst_busid is on local host, just add msg to its read channel
            if (is_local_process(msg_->dst_busid)) {
                sk::detail::channel *rc = mgr_->find_read_channel(msg_->dst_busid);
                if (!rc) {
                    sk_error("cannot get channel<%x>.", msg_->dst_busid);
                    continue;
                }

                int result = rc->push(msg_->src_busid, msg_->dst_busid, msg_->data, msg_->length);
                if (result != 0)
                    sk_error("push message error<%d>, bus<%x>.", result, msg_->dst_busid);

                continue;
            }

            // dst_busid is not on local host
            const std::string *host = find_host(msg_->dst_busid);
            if (!host) {
                sk_warn("no valid destination<%x>, initializing?", msg_->dst_busid);
                continue;
            }

            if (unlikely(*host == local_host_)) {
                sk_assert(0);
                sk::detail::channel *rc = mgr_->find_read_channel(msg_->dst_busid);
                if (!rc) {
                    sk_error("cannot get channel<%x>.", msg_->dst_busid);
                    continue;
                }

                int result = rc->push(msg_->src_busid, msg_->dst_busid, msg_->data, msg_->length);
                if (result != 0)
                    sk_error("push message error<%d>, bus<%x>.", result, msg_->dst_busid);

                continue;
            }

            // send message to remote host
            int *socket = fetch_socket(*host);
            if (!socket) continue;

            int result = nn_send(*socket, msg_, msg_->total_length(), NN_DONTWAIT);
            sk_assert(result > 0 || result == -1);
            if (result > 0) continue;

            int error = nn_errno();
            if (error != EAGAIN) {
                sk_error("send message error<%d:%s>, bus<%x>.",
                         error, nn_strerror(error), msg_->dst_busid);
                continue;
            }

            // the error is EAGAIN, try to resend the msg
            result = nn_send(*socket, msg_, msg_->total_length(), NN_DONTWAIT);
            if (result > 0) continue;

            error = nn_errno();
            sk_error("send message error<%d:%s>, bus<%x>.",
                     error, nn_strerror(error), msg_->dst_busid);

            continue;
        }

        // it should NOT get here
        sk_assert(0);
    }

    return true;
}

int *bus_router::fetch_socket(const std::string& host) {
    auto it = host2sender_.find(host);
    if (it != host2sender_.end())
        return &(it->second);

    int socket = nn_socket(AF_SP, NN_PAIR);
    if (unlikely(socket < 0)) {
        sk_error("cannot create socket, error<%d:%s>.",
                 nn_errno(), nn_strerror(nn_errno()));
        return nullptr;
    }

    char addr[128] = {0};
    snprintf(addr, sizeof(addr) - 1, "tcp://%s:%d", host.c_str(), recv_port_);
    int endpoint = nn_connect(socket, addr);
    if (endpoint < 0) {
        sk_error("cannot connect to %s, error<%d:%s>.",
                 addr, nn_errno(), nn_strerror(nn_errno()));
        return nullptr;
    }

    sk_info("connected to remote host %s.", addr);
    host2sender_[host] = socket;
    it = host2sender_.find(host);
    assert_retval(it != host2sender_.end(), nullptr);

    return &(it->second);
}

void bus_router::on_route_get_all(int ret, const consul::string_map *kv_list) {
    if (pending_count_ <= 0) sk_assert(0);
    else pending_count_ -= 1;

    if (ret != 0) {
        sk_error("consul returns error<%d>.", ret);
        return;
    }

    assert_retnone(kv_list);

    std::string prefix(BUS_KV_PREFIX);
    std::map<int, std::string> dummy;
    for (const auto& kv : *kv_list) {
        assert_continue(sk::is_prefix(prefix, kv.first));
        auto str_busid = kv.first.substr(prefix.length());
        int busid = sk::bus::from_string(str_busid.c_str());

        dummy[busid] = kv.second;

        auto it = busid2host_.find(busid);
        if (it == busid2host_.end())
            sk_info("new bus route: %s -> %s",
                    str_busid.c_str(), kv.second.c_str());
        else {
            if (it->second != kv.second)
                sk_info("update bus route: %s -> %s[old: %s]",
                        str_busid.c_str(), kv.second.c_str(), it->second.c_str());
        }
    }

    for (const auto& it : busid2host_) {
        if (dummy.find(it.first) == dummy.end()) {
            std::string str_busid = sk::bus::to_string(it.first);
            sk_info("remove bus route: %s", str_busid.c_str());
        }
    }

    busid2host_.swap(dummy);
}

void bus_router::on_route_set(int ret, const std::string& key, const std::string& value) {
    if (pending_count_ <= 0) sk_assert(0);
    else pending_count_ -= 1;

    if (ret != 0) {
        sk_error("consul returns error<%d>.", ret);
        return;
    }

    sk_assert(value == local_host_);

    std::string prefix(BUS_KV_PREFIX);
    assert_retnone(sk::is_prefix(prefix, key));

    auto str_busid = key.substr(prefix.length());
    sk_info("local bus registered: %s -> %s", str_busid.c_str(), value.c_str());

    int busid = sk::bus::from_string(str_busid.c_str());
    local_procs_.insert(busid);

    auto it = busid2host_.find(busid);
    if (it == busid2host_.end()) {
        sk_info("new bus route: %s -> %s", str_busid.c_str(), value.c_str());
        busid2host_[busid] = value;
    } else {
        if (it->second != value) {
            sk_info("update bus route: %s -> %s[old: %s]",
                    str_busid.c_str(), value.c_str(), it->second.c_str());
            it->second = value;
        }
    }
}

void bus_router::on_route_del(int ret, bool recursive, const std::string& key) {
    if (pending_count_ <= 0) sk_assert(0);
    else pending_count_ -= 1;

    if (ret != 0) {
        sk_error("consul returns error<%d>.", ret);
        return;
    }

    sk_assert(!recursive);

    std::string prefix(BUS_KV_PREFIX);
    assert_retnone(sk::is_prefix(prefix, key));

    auto str_busid = key.substr(prefix.length());
    sk_info("local bus deregistered: %s", str_busid.c_str());

    int busid = sk::bus::from_string(str_busid.c_str());
    local_procs_.erase(busid);

    sk_info("remove bus route: %s", str_busid.c_str());
    busid2host_.erase(busid);
}
