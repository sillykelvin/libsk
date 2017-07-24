#include <sstream>
#include <log/log.h>
#include <redis/redis_command.h>

NS_BEGIN(sk)

redis_command::redis_command(void *ud, const char *cmd,
                             size_t cmd_len, const std::string& key,
                             const fn_on_redis_reply& fn, const char *fmt)
    : ttl_(MAX_REQUEST_TTL), ud_(ud),
      cmd_(cmd, cmd_len), key_(key), fn_(fn) {
    std::stringstream ss;
    ss << "[ttl: " << ttl_ << ", fmt: " << fmt << ", key: " << key << ']';
    debug_str_ = ss.str();
}

redis_command_ptr redis_command::create(const std::string& key,
                                        const fn_on_redis_reply& fn,
                                        void *ud, const char *fmt, va_list ap) {
    char *cmd = nullptr;
    int cmd_len = redisvFormatCommand(&cmd, fmt, ap);
    if (unlikely(cmd_len < 0)) {
        sk_error("invalid command: %s", fmt);
        return nullptr;
    }

    redis_command *c = new redis_command(ud, cmd, cmd_len, key, fn, fmt);
    redisFreeCommand(cmd);

    if (!c) {
        sk_fatal("cannot allocate redis_command");
        return nullptr;
    }

    return redis_command_ptr(c);
}

redis_command_ptr redis_command::create(const fn_on_redis_reply& fn,
                                        void *ud, const char *fmt, va_list ap) {
    return create(std::string(), fn, ud, fmt, ap);
}

redis_command_ptr redis_command::create(const std::string& key,
                                        const fn_on_redis_reply& fn,
                                        void *ud, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    redis_command_ptr cmd = create(key, fn, ud, fmt, ap);
    va_end(ap);

    return cmd;
}

redis_command_ptr redis_command::create(const fn_on_redis_reply& fn,
                                        void *ud, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    redis_command_ptr cmd = create(fn, ud, fmt, ap);
    va_end(ap);

    return cmd;
}

void redis_command::dec_ttl() {
    ttl_ -= 1;
    auto pos = debug_str_.find(',');
    if (pos != debug_str_.npos) {
        std::stringstream ss;
        ss << "[ttl: " << ttl_ << debug_str_.substr(pos);
        debug_str_ = ss.str();
    }
}

void redis_command::on_reply(int ret, redisReply *r) {
    // this command might have been released in fn_
    // call, do NOT use any member field after that
    if (fn_) fn_(ret, shared_from_this(), r);
}

NS_END(sk)
