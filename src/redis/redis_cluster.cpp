#include <log/log.h>
#include <redis/redis_cluster.h>
#include <utility/math_helper.h>
#include <utility/string_helper.h>
#include <utility/assert_helper.h>

NS_BEGIN(sk)

static size_t get_key_slot(const std::string& key) {
    auto s = key.find('{');
    if (s != key.npos) {
        auto e = key.find('}', s + 1);
        if (e != key.npos && e != s + 1)
            return sk::crc16(key.data() + s + 1, e - s - 1) % redis_cluster::TOTAL_SLOT_COUNT;
    }

    return sk::crc16(key.data(), key.length()) % redis_cluster::TOTAL_SLOT_COUNT;
}

static int parse_host_port(const std::string& str, std::string& host, int& port) {
    auto pos = str.find(':');
    if (pos == str.npos || pos == 0 || pos == str.length() - 1)
        return -EINVAL;

    std::string p = str.substr(pos + 1);
    int ret = sk::string_traits<s32>::from_string(p.c_str(), port);
    if (ret != 0) return ret;

    host = str.substr(0, pos);
    return 0;
}

redis_cluster *redis_cluster::create(uv_loop_t *loop, const string_vector& hosts) {
    redis_cluster *cluster = new redis_cluster(loop);
    if (!cluster) return nullptr;

    for (const auto& h : hosts) {
        std::string host;
        int port = 0;
        int ret = parse_host_port(h, host, port);
        if (ret != 0) {
            sk_error("invalid host: %s", h.c_str());
            continue;
        }

        cluster->nodes_.insert(redis_node(host, port));
    }

    if (cluster->nodes_.empty()) {
        sk_fatal("at least one valid node should be provided.");
        delete cluster;
        return nullptr;
    }

    return cluster;
}

void redis_cluster::stop() {
    for (const auto& it : connections_)
        it.second->disconnect();
}

int redis_cluster::exec(const std::string& key,
                        const fn_on_redis_reply& fn,
                        void *ud, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    redis_command_ptr cmd = redis_command::create(key, fn, ud, fmt, ap);
    va_end(ap);

    if (!cmd) {
        sk_fatal("cannot create command, fmt: %s, key: %s", fmt, key.c_str());
        return -EINVAL;
    }

    return exec(cmd);
}

int redis_cluster::exec(const redis_command_ptr& cmd) {
    if (state_ == slot_invalid || state_ == slot_updating) {
        if (state_ == slot_invalid) reload_slot_cache();
        pending_commands_.push_back(cmd);
        return 0;
    }

    const redis_node *n = select_node(cmd->key());
    if (!n) {
        sk_error("cannot select a node, key: %s", cmd->c_str());
        return -EBADSLT;
    }

    return exec(cmd, *n);
}

int redis_cluster::exec(const redis_command_ptr& cmd, const redis_node& n) {
    redis_connection_ptr conn = fetch_connection(n);
    if (!conn) {
        sk_fatal("cannot fetch a connection, node: %s", n.c_str());
        return -EINVAL;
    }

    return conn->exec(cmd);
}

void redis_cluster::on_ask_received(const redis_command_ptr& cmd, redisReply *r) {
    string_vector parts;
    int ret = sk::split_string(r->str, ' ', parts);
    if (ret != 0 || parts.size() != 3) {
        sk_error("invalid reply: %s", r->str);
        return;
    }

    // int slot = 0;
    // ret = sk::string_traits<int>::from_string(parts[1].c_str(), slot);
    // if (ret != 0) {
    //     sk_error("invalid slot: %s", parts[1].c_str());
    //     return;
    // }

    std::string host;
    int port;
    ret = parse_host_port(parts[2], host, port);
    if (ret != 0) {
        sk_error("invalid host: %s", parts[2].c_str());
        return;
    }

    // make a place to hold the command to be executed after ask reply returns
    auto cb_data = new std::pair<redis_node, redis_command_ptr>(redis_node(host, port), cmd);
    if (!cb_data) {
        sk_error("OOM!");
        return;
    }

    redis_command_ptr askcmd = redis_command::create(std::bind(&redis_cluster::on_ask_reply,
                                                               this, std::placeholders::_1,
                                                               std::placeholders::_2, std::placeholders::_3),
                                                     cb_data, "ASKING");
    if (!askcmd) {
        sk_error("cannot create command ASKING.");
        delete cb_data;
        return;
    }

    ret = exec(askcmd, cb_data->first);
    if (ret != 0) {
        sk_error("cannot execute ASKING command.");
        delete cb_data;
        return;
    }
}

void redis_cluster::on_moved_received(const redis_command_ptr& cmd, redisReply *r) {
    // no matter the following logic returns error or
    // not, we force to reload the entire slot cache
    reload_slot_cache();

    string_vector parts;
    int ret = sk::split_string(r->str, ' ', parts);
    if (ret != 0 || parts.size() != 3) {
        sk_error("invalid reply: %s", r->str);
        return;
    }

    size_t slot = 0;
    ret = sk::string_traits<size_t>::from_string(parts[1].c_str(), slot);
    if (ret != 0) {
        sk_error("invalid slot: %s", parts[1].c_str());
        return;
    }

    std::string host;
    int port;
    ret = parse_host_port(parts[2], host, port);
    if (ret != 0) {
        sk_error("invalid host: %s", parts[2].c_str());
        return;
    }

    slots_[slot].reset();
    slots_[slot] = redis_node_ptr(new redis_node(host, port));

    ret = exec(cmd, *slots_[slot]);
    if (ret != 0) {
        sk_error("cannot re-execute command: %s, node: %s", cmd->c_str(), slots_[slot]->c_str());
        return;
    }
}

void redis_cluster::release_connection(const redis_connection_ptr& conn) {
    auto it = connections_.find(conn->id());
    assert_retnone(it != connections_.end());

    connections_.erase(it);
}

void redis_cluster::reload_slot_cache() {
    if (state_ == slot_updating) return;

    if (nodes_.empty()) {
        sk_fatal("at least one node should be provided.");
        return;
    }

    const redis_node& n = *nodes_.begin();
    sk_info("reloading slot cache, using node: %s", n.c_str());

    redis_command_ptr cmd = redis_command::create(std::bind(&redis_cluster::on_cluster_slot_reply,
                                                            this, std::placeholders::_1,
                                                            std::placeholders::_2, std::placeholders::_3),
                                                  nullptr, "CLUSTER SLOTS");
    if (!cmd) {
        sk_fatal("cannot create command with CLUSTER SLOTS");
        return;
    }

    int ret = exec(cmd, n);
    if (ret != 0) {
        sk_error("cannot execute CLUSTER SLOTS");
        return;
    }

    state_ = slot_updating;
    return;
}

const redis_node *redis_cluster::select_node(const std::string& key) const {
    size_t slot = static_cast<size_t>(-1);
    if (!key.empty()) {
        slot = get_key_slot(key);
        if (slots_[slot]) return slots_[slot].get();
    }

    sk_info("try selecting a random node, key: %s, slot: %lu", key.c_str(), slot);

    if (nodes_.empty()) {
        sk_error("no valid node!");
        return nullptr;
    }

    // TODO: may use std::vector<redis_node> here
    int skip = sk::random(0, nodes_.size() - 1);
    auto it = nodes_.begin();
    auto end = nodes_.end();
    while (it != end) {
        if (skip <= 0) break;
        --skip; ++it;
    }

    return &(*it);
}

redis_connection_ptr redis_cluster::fetch_connection(const redis_node& n) {
    auto it = connections_.find(n.id());
    if (it != connections_.end()) return it->second;

    redis_connection_ptr conn = redis_connection::create(loop_, n, this);
    if (!conn) {
        sk_fatal("cannot create connection: %s", n.c_str());
        return nullptr;
    }

    sk_assert(n.id() == conn->id());
    connections_.insert(std::make_pair(conn->id(), conn));
    return conn;
}

void redis_cluster::on_cluster_slot_reply(int ret, const redis_command_ptr& cmd, redisReply *r) {
    sk_assert(state_ == slot_updating);
    bool ok = false;

    do {
        if (ret != 0) {
            sk_error("reply error: %d", ret);
            break;
        }

        if (!r) {
            sk_error("null reply");
            break;
        }

        if (r->type != REDIS_REPLY_ARRAY) {
            sk_error("invalid reply: %d", r->type);
            break;
        }

        if (r->elements <= 0) {
            sk_error("no elements");
            break;
        }

        ok = true;
    } while (0);

    if (!ok) {
        nodes_.erase(nodes_.begin());
        state_ = slot_invalid;
        reload_slot_cache();
        return;
    }

    for (size_t i = 0; i < slots_.size(); ++i)
        slots_[i].reset();

    for (size_t i = 0; i < r->elements; ++i) {
        redisReply *e = r->element[i];
        if (e->elements >= 3) {
            size_t range_min = static_cast<size_t>(e->element[0]->integer);
            size_t range_max = static_cast<size_t>(e->element[1]->integer);
            redisReply *addr = e->element[2];
            if (addr->elements >= 2 &&
                addr->element[0]->type == REDIS_REPLY_STRING &&
                addr->element[0]->str[0] &&
                addr->element[1]->type == REDIS_REPLY_INTEGER) {
                redis_node n(addr->element[0]->str, static_cast<int>(addr->element[1]->integer));
                nodes_.insert(n);
                sk_info("slot update, [%lu -> %lu] -> %s",
                        range_min, range_max, n.c_str());

                for (size_t j = range_min; j <= range_max; ++j)
                    slots_[j] = redis_node_ptr(new redis_node(n.host(), n.port()));
            }
        }
    }

    state_ = slot_updated;
    while (!pending_commands_.empty()) {
        redis_command_ptr cmd = pending_commands_.front();
        pending_commands_.pop_front();

        int ret = exec(cmd);
        if (ret != 0) sk_error("cannot execute command: %s", cmd->c_str());
    }
}

void redis_cluster::on_ask_reply(int ret, const redis_command_ptr& cmd, redisReply *r) {
    auto ud = static_cast<std::pair<redis_node, redis_command_ptr>*>(cmd->user_data());
    if (ret == 0 && r && r->str && strncasecmp(r->str, "OK", 2) == 0) {
        ret = exec(ud->second, ud->first);
        if (ret != 0) sk_error("cannot execute command: %s, node: %s", ud->second->c_str(), ud->first.c_str());
        delete ud;
        return;
    }

    sk_error("redis ASKING error, ret: %d, reply: %s", ret, r && r->str ? r->str : "NONE");
    delete ud;
}

NS_END(sk)
