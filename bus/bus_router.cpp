#include <libsk.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include "bus_config.h"
#include "bus_router.h"
#include <bus/detail/channel_mgr.h>
#include <shm/detail/shm_segment.h>

#define BUS_KV_PREFIX   "bus/"
#define MURMURHASH_SEED 77

using namespace std::placeholders;

struct bus_message {
    s32 magic;
    u32 seq;
    u32 hash;
    s32 src_busid;
    s32 dst_busid;
    u32 length;
    u64 ctime;
    char data[0];

    void init(size_t capacity) {
        this->magic = MAGIC;
        this->seq = 0;
        this->hash = 0;
        this->src_busid = 0;
        this->dst_busid = 0;
        this->length = static_cast<u32>(capacity);
        this->ctime = 0;
    }

    void reset(size_t capacity) {
        length = static_cast<u32>(capacity);
    }

    size_t total_length() const {
        return sizeof(*this) + length;
    }

    bus_message *dup() const {
        size_t len = total_length();
        bus_message *msg = cast_ptr(bus_message, malloc(len));
        if (likely(msg)) mempcpy(msg, this, len);

        return msg;
    }

    void calc_hash() {
        sk::murmurhash3_x86_32(data, length, MURMURHASH_SEED, &hash);
    }

    bool verify_hash() const {
        u32 h = 0;
        sk::murmurhash3_x86_32(data, length, MURMURHASH_SEED, &h);
        return h == hash;
    }

    void ntoh() {
        magic = ntohs32(magic);
        seq = ntohu32(seq);
        hash = ntohu32(hash);
        src_busid = ntohs32(src_busid);
        dst_busid = ntohs32(dst_busid);
        length = ntohu32(length);
        ctime = ntohu64(ctime);
    }

    void hton() {
        magic = htons32(magic);
        seq = htonu32(seq);
        hash = htonu32(hash);
        src_busid = htons32(src_busid);
        dst_busid = htons32(dst_busid);
        length = htonu32(length);
        ctime = htonu64(ctime);
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

        void *tmp = &(cast_ptr(sockaddr_in, ifa->ifa_addr)->sin_addr);
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, tmp, buf, INET_ADDRSTRLEN);

        ip = buf;
        sk_info("ip of localhost: %s", ip.c_str());
    }
    freeifaddrs(addr);

    if (ip.empty()) {
        sk_error("no valid network interface.");
        return -ENODEV;
    }

    return 0;
}

bus_router::endpoint::endpoint(bus_router *r, const std::string& host, u16 port)
    : seed_(0),
      host_(host),
      client_(r->loop_, host, port,
              std::bind(&bus_router::on_server_connected, r, this, _1, _2)) {
    client_.set_read_callback(std::bind(&bus_router::on_server_message_received, r, this, _1, _2, _3));
    client_.set_write_callback(std::bind(&bus_router::on_server_message_sent, r, this, _1, _2));
}

int bus_router::endpoint::init() {
    if (connection_) return 0;
    return client_.connect();
}

void bus_router::endpoint::stop() {
    client_.stop();
    connection_.reset();
}

int bus_router::endpoint::send(bus_message *msg) {
    sk_assert(connection_);

    size_t len = msg->total_length();
    msg->magic = MAGIC;
    msg->seq = ++seed_;
    msg->calc_hash();
    msg->hton();
    ssize_t nbytes = connection_->send(msg, len);
    if (unlikely(nbytes == -1)) return -errno;

    return 0;
}

int bus_router::init(uv_loop_t *loop, sk::signal_watcher *watcher,
                     const bus_config& cfg, bool resume_mode) {
    int ret = 0;

    loop_ = loop;
    listen_port_ = static_cast<u16>(cfg.listen_port);
    loop_rate_ = (cfg.msg_per_run > 0) ? cfg.msg_per_run : 200;

    buffer_capacity_ = 2 * 1024 * 1024; // 2MB
    size_t total_len = sizeof(bus_message) + buffer_capacity_;
    msg_ = cast_ptr(bus_message, malloc(total_len));
    if (!msg_) return -ENOMEM;
    msg_->init(buffer_capacity_);

    sk::detail::shm_segment seg;
    ret = seg.init(cfg.bus_shm_key, cfg.bus_shm_size, resume_mode);
    if (ret != 0) return ret;

    mgr_ = cast_ptr(sk::detail::channel_mgr, seg.address());
    ret = mgr_->init(seg.shmid, cfg.bus_shm_size, resume_mode);

    consul_ = new sk::consul_client();
    if (!consul_) return -ENOMEM;

    ret = consul_->init(loop_, cfg.consul_addr_list);
    if (ret != 0) return ret;

    ret = consul_->watch(BUS_KV_PREFIX, 0,
                         std::bind(&bus_router::on_route_watch, this, _1, _2, _3));
    if (ret != 0) return ret;

    server_ = new sk::tcp_server(loop_, MAX_BACKLOG, listen_port_,
                                 std::bind(&bus_router::on_client_connected, this, _1, _2));
    if (!server_) return -ENOMEM;

    server_->set_read_callback(std::bind(&bus_router::on_client_message_received, this, _1, _2, _3));
    server_->set_write_callback(std::bind(&bus_router::on_client_message_sent, this, _1, _2));

    ret = server_->start();
    if (ret != 0) return ret;

    ret = watcher->watch(sk::bus::BUS_OUTGOING_SIGNO);
    if (ret != 0) return ret;

    ret = watcher->watch(sk::bus::BUS_REGISTRATION_SIGNO);
    if (ret != 0) return ret;

    ret = retrieve_local_address(localhost_);
    if (ret != 0) return ret;

    seg.release();
    return 0;
}

int bus_router::stop() {
    consul_->stop();
    server_->stop();

    for (const auto& it : host2endpoints_)
        it.second->stop();

    return 0;
}

void bus_router::fini() {
    for (const auto& it : busid2queue_) {
        for (const auto& msg : it.second)
            free(msg);
    }
    busid2queue_.clear();

    for (const auto& it : host2queue_) {
        for (const auto& msg : it.second)
            free(msg);
    }
    host2queue_.clear();

    if (msg_) {
        free(msg_);
        msg_ = nullptr;
        buffer_capacity_ = 0;
    }

    if (consul_) {
        delete consul_;
        consul_ = nullptr;
    }

    if (server_) {
        delete server_;
        server_ = nullptr;
    }

    if (mgr_) {
        shmctl(mgr_->shmid, IPC_RMID, 0);
        mgr_ = nullptr;
    }
}

void bus_router::reload(const bus_config& cfg) {
    u16 listen_port = static_cast<u16>(cfg.listen_port);
    if (listen_port != listen_port_)
        sk_warn("listening port hotfix is not supported.");

    loop_rate_ = (cfg.msg_per_run > 0) ? cfg.msg_per_run : 200;
}

void bus_router::report() const {
    sk_info("========== bus report ==========");
    sk_info("active endpoints: ");
    for (const auto& it : active_endpoints_) {
        std::string str_busid = sk::bus::to_string(it.first);
        sk_info("busid(%s) -> host(%s)", str_busid.c_str(), it.second.c_str());
    }

    std::string info("inactive endpoints: [");
    for (const auto& it : inactive_endpoints_)
        info.append(sk::bus::to_string(it)).append(" ");
    info.append("]");
    sk_info("%s", info.c_str());

    sk_info("host -> endpoints: ");
    for (const auto& it : host2endpoints_)
        sk_info("host(%s) : %s", it.first.c_str(),
                it.second->connected() ? "connected" : "not connected");

    sk_info("busid -> queue: ");
    for (const auto& it : busid2queue_) {
        std::string str_busid = sk::bus::to_string(it.first);
        sk_info("busid(%s) -> queue size(%lu)", str_busid.c_str(), it.second.size());
    }

    sk_info("host -> queue: ");
    for (const auto& it : host2queue_)
        sk_info("host(%s) -> queue size(%lu)", it.first.c_str(), it.second.size());

    mgr_->report();
    sk_info("========== bus report ==========");
}

int bus_router::handle_message(bus_message *msg) {
    // if the destination gets deregistered, just ignore the message
    if (inactive_endpoints_.find(msg->dst_busid) != inactive_endpoints_.end()) {
        sk_warn("busid %x is inactive.", msg->dst_busid);
        return -EINVAL;
    }

    const std::string *host = find_host(msg->dst_busid);

    // if host cannot be found, it must be the destination has not been
    // registered to consul, so we cache the message to send it later
    if (unlikely(!host)) {
        sk_info("host not found for %x, cache the message.", msg->dst_busid);
        enqueue(msg->dst_busid, msg);
        return 0;
    }

    // destination is on local host, send directly
    if (*host == localhost_) return send_local_message(msg);

    // the destination is not localhost
    endpoint *p = fetch_endpoint(*host);
    if (unlikely(!p)) {
        sk_error("cannot fetch endpoint: %s", host->c_str());
        return -ENOENT;
    }

    // if the remote host is connected, just send the message
    if (likely(p->connected())) {
        sk_assert(host2queue_.find(*host) == host2queue_.end());
        return p->send(msg);
    }

    // the remote is not connected, cache the message, send it later
    sk_info("endpoint %s not connected, cache the message, bus: %x",
            host->c_str(), msg->dst_busid);
    enqueue(*host, msg);
    return 0;
}

int bus_router::send_local_message(const bus_message *msg) {
    int fd = -1;
    sk::detail::channel *rc = mgr_->find_read_channel(msg->dst_busid, fd);
    if (unlikely(!rc)) {
        sk_error("cannot get channel<%x>.", msg->dst_busid);
        return -EINVAL;
    }

    if (fd < 0 || fd >= mgr_->descriptor_count) {
        sk_error("invalid descriptor<%d>.", fd);
        return -EINVAL;
    }

    int ret = rc->push(msg->src_busid, msg->dst_busid, msg->ctime, msg->data, msg->length);
    if (unlikely(ret != 0)) {
        sk_error("push message error<%d>, bus<%x>.", ret, msg->dst_busid);
        return ret;
    }

    sigval value;
    memset(&value, 0x00, sizeof(value));
    value.sival_int = fd;
    ret = sigqueue(mgr_->descriptors[fd].pid, sk::bus::BUS_INCOMING_SIGNO, value);
    if (ret != 0) sk_warn("cannot send signal: %s", strerror(errno));

    return 0;
}

const std::string *bus_router::find_host(int busid) const {
    auto it = active_endpoints_.find(busid);
    if (it != active_endpoints_.end())
        return &(it->second);

    return nullptr;
}

void bus_router::enqueue(int busid, const bus_message *msg) {
    bus_message *m = msg->dup();
    busid2queue_[busid].push_back(m);
}

void bus_router::enqueue(const std::string& host, const bus_message *msg) {
    bus_message *m = msg->dup();
    host2queue_[host].push_back(m);
}

bus_router::endpoint *bus_router::fetch_endpoint(const std::string& host) {
    assert_retval(host != localhost_, nullptr);

    auto it = host2endpoints_.find(host);
    if (it != host2endpoints_.end())
        return it->second;

    endpoint *p = new endpoint(this, host, listen_port_);
    assert_retval(p, nullptr);

    int ret = p->init();
    if (ret != 0) {
        sk_error("endpoint init error: %d", ret);
        delete p;
        return nullptr;
    }

    host2endpoints_.insert(std::make_pair(host, p));
    return p;
}

void bus_router::on_client_connected(int error, const sk::tcp_connection_ptr& conn) {
    if (error != 0) {
        sk_error("cannot accept: %s", strerror(error));
        return;
    }

    auto it = host2seq_.find(conn->remote_address().host());
    if (it != host2seq_.end()) host2seq_.erase(it);
    host2seq_[conn->remote_address().host()] = 0;

    sk_info("client %s connected.", conn->remote_address().as_string().c_str());
    conn->recv();
}

void bus_router::on_client_message_received(int error, const sk::tcp_connection_ptr& conn, sk::buffer *buf) {
    if (error == sk::tcp_connection::READ_EOF) {
        sk_info("get eof, client: %s", conn->remote_address().as_string().c_str());
        conn->close();

        auto it = host2seq_.find(conn->remote_address().host());
        sk_assert(it != host2seq_.end());
        host2seq_.erase(it);

        return;
    }

    if (error != 0) {
        sk_error("cannot read: %s", strerror(error));
        conn->close();

        auto it = host2seq_.find(conn->remote_address().host());
        sk_assert(it != host2seq_.end());
        host2seq_.erase(it);

        return;
    }

    const static size_t min_size = sizeof(bus_message);
    // NOTE: the logic in this while(...) loop is tricky, BE CAREFUL!!
    while (buf->size() >= min_size) {
        bus_message *msg = cast_ptr(bus_message, buf->mutable_peek());
        // ntoh length only, if the message is complete, then ntoh entire header
        msg->length = ntohu32(msg->length);

        if (buf->size() < msg->total_length()) {
            if (sk_trace_enabled())
                sk_trace("partial msg, src: %x, dst: %x, size: %lu, total: %lu",
                         htons32(msg->src_busid), htons32(msg->dst_busid),
                         buf->size(), msg->total_length());
            msg->length = htonu32(msg->length); // don't forget to set it back
            return;
        }

        // hton length back, then ntoh entire header
        msg->length = htonu32(msg->length);
        msg->ntoh();

        do {
            // if the magic and hash does not match, the message must be invalid
            assert_break(msg->magic == MAGIC);
            assert_break(msg->verify_hash());

            auto it = host2seq_.find(conn->remote_address().host());
            if (it == host2seq_.end()) {
                sk_assert(0);
                host2seq_[conn->remote_address().host()] = msg->seq - 1;
                it = host2seq_.find(conn->remote_address().host());
            }

            sk_assert(msg->seq == it->second + 1);
            it->second = msg->seq;

            int ret = handle_message(msg);
            if (ret != 0) sk_error("handle message error: %d, dst_busid: %x", ret, msg->dst_busid);
        } while (0);

        // no matter the message is invalid or not, we consume it
        buf->consume(msg->total_length());
    }

    conn->recv();
}

void bus_router::on_client_message_sent(int, const sk::tcp_connection_ptr& conn) {
    sk_assert(0);
    sk_fatal("this function should never be called: %s", conn->remote_address().as_string().c_str());
}

void bus_router::on_server_connected(endpoint *p, int error,
                                     const sk::tcp_connection_ptr& conn) {
    sk_assert(host2endpoints_.find(p->host()) != host2endpoints_.end());

    if (error != 0) {
        sk_fatal("cannot connect: %s, host: %s", strerror(error), p->host().c_str());
        return; // TODO: shouldn't we retry here??
    }

    sk_debug("connected to remote: %s", p->host().c_str());
    sk_assert(!p->connected());
    p->set_connection(conn);

    auto it = host2queue_.find(p->host());
    assert_retnone(it != host2queue_.end());

    message_queue& q = it->second;
    sk_debug("process queue: %s, size: %lu", p->host().c_str(), q.size());

    sk_assert(!q.empty());
    while (!q.empty()) {
        std::unique_ptr<bus_message, void(*)(void*)> msg(q.front(), ::free);
        q.pop_front();

        int ret = p->send(msg.get());
        if (ret != 0) // TODO: more robust handling here??
            sk_error("cannot send msg: %d, host: %s", ret, p->host().c_str());
    }

    host2queue_.erase(it);
}

void bus_router::on_server_message_received(endpoint *p, int error, const sk::tcp_connection_ptr& conn, sk::buffer *buf) {
    sk_assert(host2endpoints_.find(p->host()) != host2endpoints_.end());

    if (error != sk::tcp_connection::READ_EOF) {
        sk_assert(0);
        sk_fatal("this function can only be called by EOF, host: %s", p->host().c_str());
        return;
    }

    sk_info("get eof, server: %s", conn->remote_address().as_string().c_str());
    // TODO: handle eof here, reconnect??
}

void bus_router::on_server_message_sent(endpoint *p, int error, const sk::tcp_connection_ptr& conn) {
    sk_assert(host2endpoints_.find(p->host()) != host2endpoints_.end());

    if (error != 0) {
        sk_error("send error: %s, host: %s", strerror(error), p->host().c_str());
        return; // TODO: more robust handling here??
    }
}

void bus_router::on_signal(const signalfd_siginfo *info) {
    int signo = static_cast<s32>(info->ssi_signo);
    if (likely(signo == sk::bus::BUS_OUTGOING_SIGNO))
        on_local_message(info->ssi_int);
    else if (signo == sk::bus::BUS_REGISTRATION_SIGNO)
        on_descriptor_change(info->ssi_int);
    else
        sk_warn("invalid signal: %d", signo);
}

void bus_router::on_local_message(int fd) {
    if (fd < 0 || fd >= mgr_->descriptor_count) {
        sk_error("invalid descriptor<%d>.", fd);
        return;
    }

    const sk::detail::channel_descriptor& desc = mgr_->descriptors[fd];
    if (desc.closed) {
        sk_error("channel<%x> closed.", desc.owner);
        return;
    }

    sk::detail::channel *wc = mgr_->get_write_channel(fd);

    // limit the count here to avoid other channels
    // getting starved if this channel is super busy
    int count = 0;
    while (count < loop_rate_) {
        msg_->reset(buffer_capacity_);
        size_t len = msg_->length;
        int ret = wc->pop(msg_->data, len, &msg_->src_busid, &msg_->dst_busid, &msg_->ctime);
        if (ret == 0) break;

        msg_->length = static_cast<u32>(len);

        if (unlikely(ret < 0)) {
            if (ret != -E2BIG) {
                sk_error("pop message error, ret<%d>, process<%d>.", ret, desc.owner);
                continue;
            }

            sk_warn("big message, size<%lu>, buffer size<%lu>.", msg_->length, buffer_capacity_);
            bus_message *buf = cast_ptr(bus_message, malloc(msg_->total_length()));
            assert_continue(buf);

            buffer_capacity_ = msg_->length;
            buf->init(buffer_capacity_);
            free(msg_);
            msg_ = buf;

            len = msg_->length;
            ret = wc->pop(msg_->data, len, &msg_->src_busid, &msg_->dst_busid, &msg_->ctime);
            assert_continue(ret != 0);

            msg_->length = static_cast<u32>(len);

            if (unlikely(ret < 0)) {
                sk_assert(ret != -E2BIG);
                sk_error("pop message error, ret<%d>, process<%x>.", ret, desc.owner);
                continue;
            }
        }

        if (likely(ret == 1)) {
            count += 1;
            if (msg_->src_busid != desc.owner)
                sk_warn("bus mismatch, message<%x>, channel<%x>.", msg_->src_busid, desc.owner);

            int rc = handle_message(msg_);
            if (rc != 0)
                sk_error("handle message error: %d, dst_busid: %x", rc, msg_->dst_busid);

            continue;
        }

        // it should NOT get here
        sk_assert(0);
    }
}

void bus_router::on_descriptor_change(int fd) {
    if (fd < 0 || fd >= mgr_->descriptor_count) {
        sk_error("invalid descriptor<%d>.", fd);
        return;
    }

    const sk::detail::channel_descriptor& desc = mgr_->descriptors[fd];
    bool active = active_endpoints_.find(desc.owner) != active_endpoints_.end();
    bool inactive = inactive_endpoints_.find(desc.owner) != inactive_endpoints_.end();
    sk_assert((!active && !inactive) || // not registered at all
              (active  && !inactive) || // registered
              (!active && inactive));   // deretistered

    // channel registered, but not added to consul
    if (!active && !desc.closed) {
        std::string key(BUS_KV_PREFIX + sk::bus::to_string(desc.owner));
        sk_info("registering key %s to consul...", key.c_str());
        consul_->set(key, localhost_, std::bind(&bus_router::on_route_set, this, _1, _2, _3));
        return;
    }

    // channel closed, but not removed from consul
    if (active && desc.closed) {
        std::string key(BUS_KV_PREFIX + sk::bus::to_string(desc.owner));
        sk_debug("deregistering key %s from consul...", key.c_str());
        consul_->del(key, false, std::bind(&bus_router::on_route_del, this, _1, _2, _3));
        return;
    }
}

void bus_router::on_route_watch(int ret, int index,
                                const std::map<std::string, std::string> *kv_list) {
    if (ret != 0) {
        if (ret == -ENOENT)
            sk_debug("consul returns 404.");
        else
            sk_error("consul returns error: %d", ret);

        ret = consul_->watch(BUS_KV_PREFIX, index,
                             std::bind(&bus_router::on_route_watch, this, _1, _2, _3));
        if (ret != 0) sk_error("consul watch error: %d", ret);
        return;
    }

    assert_retnone(kv_list);

    std::string prefix(BUS_KV_PREFIX);
    std::map<int, std::string> active;
    for (const auto& kv : *kv_list) {
        assert_continue(sk::is_prefix(prefix, kv.first));
        std::string str_busid = kv.first.substr(prefix.length());
        int busid = sk::bus::from_string(str_busid.c_str());

        active[busid] = kv.second;
        inactive_endpoints_.erase(busid);

        // the existing registration must NOT have a queue
        if (active_endpoints_.find(busid) != active_endpoints_.end())
            sk_assert(busid2queue_.find(busid) == busid2queue_.end());
        else
            sk_info("new bus route: %s", str_busid.c_str());
    }

    for (const auto& it : active_endpoints_) {
        if (active.find(it.first) == active.end()) {
            std::string str_busid = sk::bus::to_string(it.first);
            sk_info("remove bus route: %s", str_busid.c_str());

            inactive_endpoints_.insert(it.first);
        }
    }
    active_endpoints_.swap(active);

    for (const auto& it : active_endpoints_) {
        int busid = it.first;
        auto sit = busid2queue_.find(busid);
        if (sit == busid2queue_.end())
            continue;

        message_queue& q = sit->second;
        sk_debug("process queue: %x, size: %lu", busid, q.size());
        while (!q.empty()) {
            std::unique_ptr<bus_message, void(*)(void*)> msg(q.front(), ::free);
            q.pop_front();

            int ret = handle_message(msg.get());
            if (ret != 0)
                sk_error("handle message error: %d, dst: %x", ret, msg->dst_busid);
        }

        busid2queue_.erase(sit);
    }

    ret = consul_->watch(BUS_KV_PREFIX, index,
                         std::bind(&bus_router::on_route_watch, this, _1, _2, _3));
    if (ret != 0) sk_error("consul watch error: %d", ret);
}

void bus_router::on_route_set(int ret, const std::string& key, const std::string& value) {
    if (ret != 0) {
        sk_error("consul returns error: %d", ret);
        return;
    }

    sk_assert(value == localhost_);

    std::string prefix(BUS_KV_PREFIX);
    assert_retnone(sk::is_prefix(prefix, key));

    std::string str_busid = key.substr(prefix.length());
    sk_info("local bus registered: %s -> %s", str_busid.c_str(), value.c_str());

    int busid = sk::bus::from_string(str_busid.c_str());
    bool existing = active_endpoints_.find(busid) != active_endpoints_.end();

    active_endpoints_[busid] = value;
    inactive_endpoints_.erase(busid);

    auto it = busid2queue_.find(busid);
    if (existing) sk_assert(it == busid2queue_.end());
    if (it == busid2queue_.end()) return;

    message_queue& q = it->second;
    sk_debug("process queue: %x, size: %lu", busid, q.size());
    while (!q.empty()) {
        std::unique_ptr<bus_message, void(*)(void*)> msg(q.front(), ::free);
        q.pop_front();

        int ret = send_local_message(msg.get());
        if (ret != 0)
            sk_error("cannot send local message: %d, dst: %x", ret, msg->dst_busid);
    }

    busid2queue_.erase(it);
}

void bus_router::on_route_del(int ret, bool recursive, const std::string& key) {
    if (ret != 0) {
        sk_error("consul returns error: %d", ret);
        return;
    }

    sk_assert(!recursive);

    std::string prefix(BUS_KV_PREFIX);
    assert_retnone(sk::is_prefix(prefix, key));

    std::string str_busid = key.substr(prefix.length());
    sk_info("local bus deregistered: %s", str_busid.c_str());

    int busid = sk::bus::from_string(str_busid.c_str());
    active_endpoints_.erase(busid);
    inactive_endpoints_.insert(busid);
}
