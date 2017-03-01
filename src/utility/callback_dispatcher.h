#ifndef CALLBACK_DISPATCHER_H
#define CALLBACK_DISPATCHER_H

#include <map>
#include "utility/singleton.h"
#include "utility/string_helper.h"
#include "utility/assert_helper.h"

namespace sk {

/*
 * type here is used to distinguish those dispatchers who have
 * same K and same Fn signature, but have different usages, so
 * we can make different singleton instances by different types
 */
template<int type, typename K, typename Fn>
class callback_dispatcher;

template<int type, typename K, typename R, typename... Args>
class callback_dispatcher<type, K, R(Args...)> {
    DECLARE_SINGLETON(callback_dispatcher);
public:
    typedef R (*fn_handler)(Args...);

    ~callback_dispatcher() {}

    bool has_handler(const K& key) const {
        return handlers_.find(key) != handlers_.end();
    }

    void register_handler(const K& key, fn_handler handler) {
        auto it = handlers_.find(key);
        assert_retnone(it == handlers_.end());

        handlers_[key] = handler;
    }

    R dispatch(const K& key, Args... args) {
        auto it = handlers_.find(key);
        if (it == handlers_.end()) {
            sk_assert(0);
            std::string str = string_traits<K>::to_string(key);
            sk_error("handler not found: %s", str.c_str());

            return R();
        }

        return it->second(args...);
    }

private:
    callback_dispatcher() {}

    std::map<K, fn_handler> handlers_;
};

template<int type, typename K, typename... Args>
class callback_dispatcher<type, K, void(Args...)> {
    DECLARE_SINGLETON(callback_dispatcher);
public:
    typedef void (*fn_handler)(Args...);

    ~callback_dispatcher() {}

    bool has_handler(const K& key) const {
        return handlers_.find(key) != handlers_.end();
    }

    void register_handler(const K& key, fn_handler handler) {
        auto it = handlers_.find(key);
        assert_retnone(it == handlers_.end());

        handlers_[key] = handler;
    }

    void dispatch(const K& key, Args... args) {
        auto it = handlers_.find(key);
        if (it == handlers_.end()) {
            sk_assert(0);
            std::string str = string_traits<K>::to_string(key);
            sk_error("handler not found: %s", str.c_str());

            return;
        }

        return it->second(args...);
    }

private:
    callback_dispatcher() {}

    std::map<K, fn_handler> handlers_;
};

} // namespace sk

#endif // CALLBACK_DISPATCHER_H
