#ifndef REDIS_CLUSTER_H
#define REDIS_CLUSTER_H

#include <set>
#include <list>
#include <array>
#include <core/callback.h>
#include <redis/redis_node.h>
#include <redis/redis_command.h>
#include <redis/redis_connection.h>

NS_BEGIN(sk)

class redis_cluster {
public:
    static const int TOTAL_SLOT_COUNT = 16384;

    static redis_cluster *create(uv_loop_t *loop, const string_vector& hosts);
    ~redis_cluster() = default;

    void stop();

    int exec(const std::string& key,
             const fn_on_redis_reply& fn,
             void *ud, const char *fmt, ...);
    int exec(const redis_command_ptr& cmd);
    int exec(const redis_command_ptr& cmd, const redis_node& n);

    void on_ask_received(const redis_command_ptr& cmd, redisReply *r);
    void on_moved_received(const redis_command_ptr& cmd, redisReply *r);

    void release_connection(const redis_connection_ptr& conn);

private:
    redis_cluster(uv_loop_t *loop) : state_(slot_invalid), loop_(loop) {}

    void reload_slot_cache();
    const redis_node *select_node(const std::string& key) const;
    redis_connection_ptr fetch_connection(const redis_node& n);

private:
    void on_cluster_slot_reply(int ret, const redis_command_ptr& cmd, redisReply *r);
    void on_ask_reply(int ret, const redis_command_ptr& cmd, redisReply *r);

private:
    enum slot_state { slot_invalid, slot_updating, slot_updated };

    slot_state state_;
    uv_loop_t *loop_;

    // all nodes returned from "CLUSTER SLOTS"
    std::set<redis_node> nodes_;
    // slot index -> current valid master node
    std::array<redis_node_ptr, TOTAL_SLOT_COUNT> slots_;
    // node id -> node connection
    std::map<std::string, redis_connection_ptr> connections_;

    // pending commands to be sent (the slots MUST be updating)
    std::list<redis_command_ptr> pending_commands_;

    MAKE_NONCOPYABLE(redis_cluster);
};

NS_END(sk)

#endif // REDIS_CLUSTER_H
