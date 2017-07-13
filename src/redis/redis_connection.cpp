#include <log/log.h>
#include <core/callback.h>
#include <redis/redis_node.h>
#include <redis/redis_cluster.h>
#include <utility/assert_helper.h>
#include <utility/string_helper.h>
#include <redis/redis_connection.h>
#include <hiredis/adapters/libuv.h>

NS_BEGIN(sk)

redis_connection::redis_connection(redis_cluster *cluster,
                                   redisAsyncContext *ctx,
                                   state s, const redis_node& n)
    : state_(s), id_(n.id()), ctx_(ctx), cluster_(cluster) {}

redis_connection::~redis_connection() {
    if (state_ != disconnected) sk_warn("active state: %d", state_);
}

redis_connection_ptr redis_connection::create(uv_loop_t *loop,
                                              const redis_node& n,
                                              redis_cluster *cluster) {
    redisAsyncContext *ctx = redisAsyncConnect(n.host().c_str(), n.port());
    if (!ctx) {
        sk_fatal("cannot allocate redisAsyncContext");
        return nullptr;
    }

    if (ctx->err) {
        sk_fatal("redis connection error: %s", ctx->errstr);
        redisAsyncFree(ctx);
        return nullptr;
    }

    int ret = redisLibuvAttach(ctx, loop);
    if (ret != REDIS_OK) {
        sk_fatal("cannot attach to libuv: %d", ret);
        redisAsyncFree(ctx);
        return nullptr;
    }

    redisAsyncSetConnectCallback(ctx, on_connected);
    redisAsyncSetDisconnectCallback(ctx, on_disconnected);

    redis_connection *conn = new redis_connection(cluster, ctx, connecting, n);
    if (!conn) {
        sk_fatal("cannot allocate connection.");
        redisAsyncFree(ctx);
        return nullptr;
    }

    ctx->data = conn;
    return redis_connection_ptr(conn);
}

void redis_connection::disconnect() {
    if (state_ == disconnected || state_ == disconnecting)
        return;

    redisAsyncDisconnect(ctx_);
    state_ = disconnecting;
}

int redis_connection::exec(const redis_command_ptr& cmd) {
    if (cmd->ttl() <= 0) {
        sk_error("ttl of command %s expired.", cmd->c_str());
        return -ELOOP;
    }

    if (state_ == disconnecting || state_ == disconnected) {
        sk_error("connection %s is disconnecting(ed), state: %d", c_str(), state_);
        return -EINVAL;
    }

    int ret = redisAsyncFormattedCommand(ctx_, on_reply, cmd.get(), cmd->cmd(), cmd->length());
    if (ret != REDIS_OK) {
        sk_error("cannot execute command, ret: %d, connection: %s, cmd: %s", ret, c_str(), cmd->c_str());
        return ret;
    }

    cmd->dec_ttl();

    // hold the commands here, to avoid it getting
    // released before the reply returns
    sk_assert(running_commands_.find(cmd) == running_commands_.end());
    running_commands_.insert(cmd);

    sk_debug("executing command %s, connection: %s", cmd->c_str(), c_str());
    return 0;
}

void redis_connection::on_reply(redisAsyncContext *ctx, void *r, void *ud) {
    redis_connection *rawconn = static_cast<redis_connection*>(ctx->data);
    redis_command *rawcmd = static_cast<redis_command*>(ud);
    redisReply *reply = static_cast<redisReply*>(r);

    // hold the two resources here, to avoid they getting
    // released in the following callback logic
    redis_connection_ptr conn = rawconn->shared_from_this();
    redis_command_ptr cmd = rawcmd->shared_from_this();

    auto it = conn->running_commands_.find(cmd);
    sk_assert(it != conn->running_commands_.end());
    conn->running_commands_.erase(it);

    if (ctx->err != REDIS_OK) {
        sk_error("redis error %s, code: %d, connection: %s, cmd: %s",
                 ctx->errstr, ctx->err, conn->c_str(), cmd->c_str());
        return cmd->on_reply(ctx->err, reply);
    }

    if (!reply) {
        sk_error("null reply, connection: %s, cmd: %s", conn->c_str(), cmd->c_str());
        return cmd->on_reply(-EIO, reply);
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        if (strncasecmp(reply->str, "ASK", 3) == 0) {
            sk_info("ASK reply received: %s, connection: %s, cmd: %s",
                    reply->str, conn->c_str(), cmd->c_str());
            conn->cluster_->on_ask_received(cmd->shared_from_this(), reply);
            return;
        }

        if (strncasecmp(reply->str, "MOVED", 5) == 0) {
            sk_info("MOVED reply received: %s, connection: %s, cmd: %s",
                    reply->str, conn->c_str(), cmd->c_str());
            conn->cluster_->on_moved_received(cmd->shared_from_this(), reply);
            return;
        }

        if (strncasecmp(reply->str, "CLUSTERDOWN", 11) == 0) {
            sk_error("the cluster is down! connection: %s, cmd: %s",
                     conn->c_str(), cmd->c_str());
            cmd->on_reply(-EREMOTEIO, reply);
            return;
        }

        sk_error("reply error: %s, connection: %s, cmd: %s",
                 reply->str, conn->c_str(), cmd->c_str());
        cmd->on_reply(-EREMOTEIO, reply);
        return;
    }

    sk_debug("reply received, type: %d, connection: %s, cmd: %s",
             reply->type, conn->c_str(), cmd->c_str());
    cmd->on_reply(0, reply);
}

void redis_connection::on_connected(const redisAsyncContext *ctx, int status) {
    redis_connection *conn = static_cast<redis_connection*>(ctx->data);
    if (status != REDIS_OK) {
        sk_fatal("cannot connect to %s", conn->c_str());
        conn->cluster_->release_connection(conn->shared_from_this());
        // conn->cluster_->reload_slots_cache();
        return;
    }

    conn->state_ = connected;
    sk_info("connected to %s", conn->c_str());
}

void redis_connection::on_disconnected(const redisAsyncContext *ctx, int status) {
    redis_connection *conn = static_cast<redis_connection*>(ctx->data);
    if (status != REDIS_OK)
        sk_fatal("disconnect error, connection: %s", conn->c_str());

    conn->state_ = disconnected;
    sk_info("connection disconnected: %s", conn->c_str());
    conn->cluster_->release_connection(conn->shared_from_this());
}

NS_END(sk)
