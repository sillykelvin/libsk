#include <json.hpp>
#include <log/log.h>
#include <core/consul_client.h>
#include <utility/assert_helper.h>
#include <utility/string_helper.h>

#define DEFAULT_WATCH_TIME "10m"
#define KV_STATIC_PREFIX   "/v1/kv/"
#define INDEX_HEADER_NAME  "X-Consul-Index"

using namespace std::placeholders;

struct consul_set_context {
    std::string key;
    std::string value;

    consul_set_context(const std::string& key, const std::string& value)
        : key(key), value(value) {}
};

struct consul_del_context {
    bool recursive;
    std::string key;

    consul_del_context(bool recursive, const std::string& key)
        : recursive(recursive), key(key) {}
};

template<typename T>
struct guard {
public:
    guard(T *t) : t_(t) {}
    ~guard() { if (t_) delete t_; }
private:
    T *t_;
};

NS_BEGIN(sk)

int consul_client::init(uv_loop_t *loop, const string_vector& addr_list) {
    if (addr_list.empty()) {
        sk_error("at least one consul address should be provided.");
        return -1;
    }

    master_ = std::make_shared<rest_client>(loop, addr_list[0].c_str());
    assert_retval(master_, -1);

    master_->set_transfer_timeout(0);

    for (size_t i = 1; i < addr_list.size(); ++i) {
        auto backup = std::make_shared<rest_client>(loop, addr_list[i].c_str());
        assert_continue(backup);

        backup->set_transfer_timeout(0);
        backup_.push_back(backup);
    }

    return 0;
}

void consul_client::stop() {
    master_->stop();
}

int consul_client::watch(const std::string& prefix, int index, const fn_on_consul_watch& fn) {
    std::string uri(KV_STATIC_PREFIX + prefix);

    string_map parameters;
    parameters["recurse"] = "";
    parameters["index"] = std::to_string(index);
    parameters["wait"] = DEFAULT_WATCH_TIME;

    return master_->get(uri.c_str(),
                        std::bind(&consul_client::on_watch, this, _1, _2, _3, _4, fn),
                        &parameters, nullptr);
}

int consul_client::set(const std::string& key, const std::string& value, const fn_on_consul_set& fn) {
    if (key.empty()) {
        sk_error("invalid key.");
        return -EINVAL;
    }

    std::string uri(KV_STATIC_PREFIX + key);
    consul_set_context *ctx = new consul_set_context(key, value);
    assert_retval(ctx, -1);

    return master_->put(uri.c_str(),
                        std::bind(&consul_client::on_set, this, ctx, _1, _2, _3, _4, fn),
                        value.data(), value.length(),
                        nullptr, nullptr);
}

int consul_client::del(const std::string& key, bool recursive, const fn_on_consul_del& fn) {
    if (key.empty()) {
        sk_error("invalid key.");
        return -EINVAL;
    }

    std::string uri(KV_STATIC_PREFIX + key);
    consul_del_context *ctx = new consul_del_context(recursive, key);
    assert_retval(ctx, -1);

    string_map parameters;
    if (recursive) parameters["recurse"] = "";

    return master_->del(uri.c_str(),
                        std::bind(&consul_client::on_del, this, ctx, _1, _2, _3, _4, fn),
                        parameters.empty() ? nullptr : &parameters, nullptr);
}

void consul_client::on_watch(int ret, int status,
                             const string_map& headers,
                             const std::string& body,
                             const fn_on_consul_watch& fn) {
    if (ret != 0) {
        sk_error("curl error<%d>.", ret);
        fn(ret, 0, nullptr);
        return;
    }

    sk_trace("status: %d, body: %s", status, body.c_str());

    // althrough the status might not be 200(OK), the headers
    // are always presented, so no matter it succeeded or not,
    // we parse the index from headers and set it to callback

    auto it = headers.find(INDEX_HEADER_NAME);
    if (it == headers.end()) {
        sk_error("required header not found.");
        ret = -EINVAL;
        fn(ret, 0, nullptr);
        return;
    }

    int index = 0;
    int rc = sk::string_traits<int>::from_string(it->second.c_str(), index);
    if (rc != 0) {
        sk_error("cannot convert %s to int.", it->second.c_str());
        ret = -EINVAL;
        fn(ret, 0, nullptr);
        return;
    }

    if (status == 404) {
        ret = -ENOENT;
        fn(ret, index, nullptr);
        return;
    }

    if (status == 400) {
        ret = -EINVAL;
        fn(ret, index, nullptr);
        return;
    }

    if (status != 200) {
        sk_error("unknown status code<%d>.", status);
        ret = -EFAULT;
        fn(ret, index, nullptr);
        return;
    }

    try {
        auto object = nlohmann::json::parse(body);
        if (!object.is_array()) {
            sk_error("invalid body: %s", body.c_str());
            ret = -EINVAL;
            fn(ret, index, nullptr);
            return;
        }

        string_map kv;
        for (const auto& it : object) {
            if (!it.is_object()) {
                sk_error("invalid body: %s", body.c_str());
                ret = -EINVAL;
                fn(ret, index, nullptr);
                return;
            }

            auto kit = it.find("Key");
            if (kit == it.end() || !kit->is_string()) {
                sk_error("invalid body: %s", body.c_str());
                ret = -EINVAL;
                fn(ret, index, nullptr);
                return;
            }

            auto vit = it.find("Value");
            if (vit == it.end() || !vit->is_string()) {
                sk_error("invalid body: %s", body.c_str());
                ret = -EINVAL;
                fn(ret, index, nullptr);
                return;
            }

            kv[*kit] = sk::base64_decode(*vit);
        }

        fn(0, index, &kv);
    } catch (...) {
        sk_fatal("exception caught, body: %s", body.c_str());
        ret = -EINVAL;
        fn(ret, index, nullptr);
        return;
    }
}

void consul_client::on_set(consul_set_context *ctx,
                           int ret, int status,
                           const string_map& headers,
                           const std::string& body,
                           const fn_on_consul_set& fn) {
    (void) headers;
    guard<consul_set_context> g(ctx);

    if (ret != 0) {
        sk_error("curl error<%d>.", ret);
        fn(ret, ctx->key, ctx->value);
        return;
    }

    sk_trace("status: %d, body: %s", status, body.c_str());

    if (status == 404) {
        ret = -ENOENT;
        fn(ret, ctx->key, ctx->value);
        return;
    }

    if (status == 400) {
        ret = -EINVAL;
        fn(ret, ctx->key, ctx->value);
        return;
    }

    if (status != 200) {
        sk_error("unknown status code<%d>.", status);
        ret = -EFAULT;
        fn(ret, ctx->key, ctx->value);
        return;
    }

    try {
        auto object = nlohmann::json::parse(body);
        if (!object.is_boolean()) {
            sk_error("invalid body: %s", body.c_str());
            ret = -EINVAL;
            fn(ret, ctx->key, ctx->value);
            return;
        }

        if (!object) {
            sk_error("set returns false.");
            ret = -EAGAIN;
            fn(ret, ctx->key, ctx->value);
            return;
        }

        fn(0, ctx->key, ctx->value);
    } catch (...) {
        sk_fatal("exception caught, body: %s", body.c_str());
        ret = -EINVAL;
        fn(ret, ctx->key, ctx->value);
        return;
    }
}

void consul_client::on_del(consul_del_context *ctx,
                           int ret, int status,
                           const string_map& headers,
                           const std::string& body,
                           const fn_on_consul_del& fn) {
    (void) headers;
    guard<consul_del_context> g(ctx);

    if (ret != 0) {
        sk_error("curl error<%d>.", ret);
        fn(ret, ctx->recursive, ctx->key);
        return;
    }

    sk_trace("status: %d, body: %s", status, body.c_str());

    if (status == 404) {
        ret = -ENOENT;
        fn(ret, ctx->recursive, ctx->key);
        return;
    }

    if (status == 400) {
        ret = -EINVAL;
        fn(ret, ctx->recursive, ctx->key);
        return;
    }

    if (status != 200) {
        sk_error("unknown status code<%d>.", status);
        ret = -EFAULT;
        fn(ret, ctx->recursive, ctx->key);
        return;
    }

    try {
        auto object = nlohmann::json::parse(body);
        if (!object.is_boolean()) {
            sk_error("invalid body: %s", body.c_str());
            ret = -EINVAL;
            fn(ret, ctx->recursive, ctx->key);
            return;
        }

        if (!object) {
            sk_error("del returns false.");
            ret = -EAGAIN;
            fn(ret, ctx->recursive, ctx->key);
            return;
        }

        fn(0, ctx->recursive, ctx->key);
    } catch (...) {
        sk_fatal("exception caught, body: %s", body.c_str());
        ret = -EINVAL;
        fn(ret, ctx->recursive, ctx->key);
        return;
    }
}

NS_END(sk)
