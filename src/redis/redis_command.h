#ifndef REDIS_COMMAND_H
#define REDIS_COMMAND_H

#include <memory>
#include <utility/types.h>
#include <hiredis/hiredis.h>

NS_BEGIN(sk)

class redis_command;
using redis_command_ptr = std::shared_ptr<redis_command>;
using fn_on_redis_reply = std::function<void(int ret, const redis_command_ptr& cmd, redisReply *reply)>;

class redis_command : public std::enable_shared_from_this<redis_command> {
public:
    static redis_command_ptr create(const std::string& key,
                                    const fn_on_redis_reply& fn,
                                    void *ud, const char *fmt, va_list ap);

    static redis_command_ptr create(const fn_on_redis_reply& fn,
                                    void *ud, const char *fmt, va_list ap);

    static redis_command_ptr create(const std::string& key,
                                    const fn_on_redis_reply& fn,
                                    void *ud, const char *fmt, ...);

    static redis_command_ptr create(const fn_on_redis_reply& fn,
                                    void *ud, const char *fmt, ...);

    ~redis_command() = default;

    void dec_ttl() { ttl_ -= 1; }

    void on_reply(int ret, redisReply *r);

    int ttl() const { return ttl_; }
    const std::string& key() const { return key_; }
    const char *c_str() const { return debug_str_.c_str(); }
    const char *cmd() const { return cmd_.data(); }
    size_t length() const { return cmd_.length(); }
    void *user_data() const { return ud_; }

private:
    redis_command(void *ud, const char *cmd,
                  size_t cmd_len, const std::string& key,
                  const fn_on_redis_reply& fn, const char *fmt);

private:
    static const int MAX_REQUEST_TTL = 16;

    int ttl_;
    void *ud_;
    std::string cmd_;
    std::string key_;
    fn_on_redis_reply fn_;
    std::string debug_str_;

    MAKE_NONCOPYABLE(redis_command);
};

NS_END(sk)

#endif // REDIS_COMMAND_H
