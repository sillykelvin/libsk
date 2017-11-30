#ifndef REDIS_CONNECTION_H
#define REDIS_CONNECTION_H

#include <set>
#include <uv.h>
#include <hiredis/async.h>
#include <redis/redis_command.h>

NS_BEGIN(sk)

class redis_node;
class redis_cluster;
class redis_connection;
using redis_connection_ptr = std::shared_ptr<redis_connection>;

class redis_connection : public std::enable_shared_from_this<redis_connection> {
public:
    static redis_connection_ptr create(uv_loop_t *loop,
                                       const redis_node& n,
                                       redis_cluster *cluster);
    ~redis_connection();

    void disconnect();

    bool disconnected()  const { return state_ == state_disconnected; }
    bool disconnecting() const { return state_ == state_disconnecting; }

    int exec(const redis_command_ptr& cmd);

    const std::string& id() const { return id_; }
    const char *c_str() const { return id_.c_str(); }

private:
    enum state {
        state_connecting, state_connected,
        state_disconnecting, state_disconnected
    };

private:
    redis_connection(redis_cluster *cluster,
                     redisAsyncContext *ctx,
                     state s, const redis_node& n);

    static void on_reply(redisAsyncContext *ctx, void *r, void *ud);
    static void on_connected(const redisAsyncContext *ctx, int status);
    static void on_disconnected(const redisAsyncContext *ctx, int status);

private:
    state state_;
    std::string id_;
    redisAsyncContext *ctx_;
    redis_cluster *cluster_;
    std::set<redis_command_ptr> running_commands_;

    MAKE_NONCOPYABLE(redis_connection);
};

NS_END(sk)

#endif // REDIS_CONNECTION_H
