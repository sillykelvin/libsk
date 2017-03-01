#include "consul.h"
#include "json.hpp"
#include "tk_errcode.pb.h"
#include "utility/string_helper.h"

#define KV_STATIC_PREFIX "/v1/kv/"

struct consul_get_all_ctx {
    consul::fn_on_get_all cb;
};

struct consul_get_ctx {
    std::string key;
    consul::fn_on_get cb;
};

struct consul_set_ctx {
    std::string key;
    std::string value;
    consul::fn_on_set cb;
};

struct consul_del_ctx {
    bool recursive;
    std::string key;
    consul::fn_on_del cb;
};

int consul::init(const string_vector& addr_list) {
    if (addr_list.empty()) {
        sk_error("at least one consul address should be provided.");
        return -1;
    }

    master_client_ = std::make_shared<rest_client>(addr_list[0].c_str());
    assert_retval(master_client_, -1);

    for (size_t i = 1; i < addr_list.size(); ++i)
        backup_clients_.push_back(std::make_shared<rest_client>(addr_list[i].c_str()));

    return 0;
}

int consul::get_all(const std::string& prefix, fn_on_get_all cb) {
    std::string uri(KV_STATIC_PREFIX + prefix);

    string_map parameters;
    parameters["recurse"] = "";

    consul_get_all_ctx *ctx = new consul_get_all_ctx();
    assert_retval(ctx, -1);
    ctx->cb = cb;

    return master_client_->get(uri.c_str(), consul::on_get_all,
                               &parameters, nullptr, &ctx, sizeof(ctx));
}

int consul::get(const std::string& key, fn_on_get cb) {
    if (key.empty()) {
        sk_error("invalid key.");
        return TKERR_INVALID_ARG;
    }

    std::string uri(KV_STATIC_PREFIX + key);

    consul_get_ctx *ctx = new consul_get_ctx();
    assert_retval(ctx, -1);
    ctx->key = key;
    ctx->cb = cb;

    return master_client_->get(uri.c_str(), consul::on_get,
                               nullptr, nullptr, &ctx, sizeof(ctx));
}

int consul::set(const std::string& key, const std::string& value, fn_on_set cb) {
    if (key.empty()) {
        sk_error("invalid key.");
        return TKERR_INVALID_ARG;
    }

    std::string uri(KV_STATIC_PREFIX + key);

    consul_set_ctx *ctx = new consul_set_ctx();
    assert_retval(ctx, -1);
    ctx->key = key;
    ctx->value = value;
    ctx->cb = cb;

    return master_client_->put(uri.c_str(), consul::on_set,
                               nullptr, nullptr,
                               value.data(), value.length(),
                               &ctx, sizeof(ctx));
}

int consul::del(const std::string& key, bool recursive, consul::fn_on_del cb) {
    if (key.empty()) {
        sk_error("invalid key.");
        return TKERR_INVALID_ARG;
    }

    std::string uri(KV_STATIC_PREFIX + key);

    consul_del_ctx *ctx = new consul_del_ctx();
    assert_retval(ctx, -1);
    ctx->key = key;
    ctx->recursive = recursive;
    ctx->cb = cb;

    string_map parameters;
    if (recursive) parameters["recurse"] = "";

    return master_client_->del(uri.c_str(), consul::on_del,
                               parameters.empty() ? nullptr : &parameters,
                               nullptr, &ctx, sizeof(ctx));
}

void consul::run() {
    if (master_client_)
        master_client_->run();
}

int consul::on_get_all(int ret, int status,
                       const std::string& response,
                       const void *cb_data, int cb_len) {
    consul_get_all_ctx *ctx = nullptr;
    assert_retval(cb_data && cb_len == sizeof(ctx), -1);
    ctx = (consul_get_all_ctx *) (*((uintptr_t *) cb_data));

    struct guard {
        guard(consul_get_all_ctx *ctx) { this->ctx = ctx; }
        ~guard() { delete ctx; }

        consul_get_all_ctx *ctx;
    } g(ctx);

    if (ret != 0) {
        sk_error("curl error<%d>.", ret);
        ctx->cb(ret, nullptr);
        return ret;
    }

    sk_debug("status: %d, response: %s", status, response.c_str());

    if (status == 404) {
        ret = TKERR_NOTEXIST;
        ctx->cb(ret, nullptr);
        return ret;
    }

    if (status == 400) {
        ret = TKERR_INVALID_ARG;
        ctx->cb(ret, nullptr);
        return ret;
    }

    if (status != 200) {
        sk_error("unknown status code<%d>.", status);
        ret = TKERR_UNKNOWN;
        ctx->cb(ret, nullptr);
        return ret;
    }

    try {
        auto object = nlohmann::json::parse(response);
        if (!object.is_array()) {
            sk_error("invalid response: %s", response.c_str());
            ret = TKERR_ILLEGAL_PKG;
            ctx->cb(ret, nullptr);
            return ret;
        }

        string_map kv;
        for (const auto& it : object) {
            if (!it.is_object()) {
                sk_error("invalid response: %s", response.c_str());
                ret = TKERR_ILLEGAL_PKG;
                ctx->cb(ret, nullptr);
                return ret;
            }

            auto kit = it.find("Key");
            if (kit == it.end() || !kit->is_string()) {
                sk_error("invalid response: %s", response.c_str());
                ret = TKERR_ILLEGAL_PKG;
                ctx->cb(ret, nullptr);
                return ret;
            }

            auto vit = it.find("Value");
            if (vit == it.end() || !vit->is_string()) {
                sk_error("invalid response: %s", response.c_str());
                ret = TKERR_ILLEGAL_PKG;
                ctx->cb(ret, nullptr);
                return ret;
            }

            kv[*kit] = sk::base64_decode(*vit);
        }

        ctx->cb(0, &kv);
    } catch (...) {
        sk_fatal("exception caught, response: %s", response.c_str());
        ret = TKERR_ILLEGAL_PKG;
        ctx->cb(ret, nullptr);
        return ret;
    }

    return 0;
}

int consul::on_get(int ret, int status,
                   const std::string& response,
                   const void *cb_data, int cb_len) {
    consul_get_ctx *ctx = nullptr;
    assert_retval(cb_data && cb_len == sizeof(ctx), -1);
    ctx = (consul_get_ctx *) (*((uintptr_t *) cb_data));

    struct guard {
        guard(consul_get_ctx *ctx) { this->ctx = ctx; }
        ~guard() { delete ctx; }

        consul_get_ctx *ctx;
    } g(ctx);

    if (ret != 0) {
        sk_error("curl error<%d>.", ret);
        ctx->cb(ret, ctx->key, nullptr);
        return ret;
    }

    sk_debug("status: %d, response: %s", status, response.c_str());

    if (status == 404) {
        ret = TKERR_NOTEXIST;
        ctx->cb(ret, ctx->key, nullptr);
        return ret;
    }

    if (status == 400) {
        ret = TKERR_INVALID_ARG;
        ctx->cb(ret, ctx->key, nullptr);
        return ret;
    }

    if (status != 200) {
        sk_error("unknown status code<%d>.", status);
        ret = TKERR_UNKNOWN;
        ctx->cb(ret, ctx->key, nullptr);
        return ret;
    }

    try {
        auto object = nlohmann::json::parse(response);
        if (!object.is_object()) {
            sk_error("invalid response: %s", response.c_str());
            ret = TKERR_ILLEGAL_PKG;
            ctx->cb(ret, ctx->key, nullptr);
            return ret;
        }

        auto kit = object.find("Key");
        if (kit == object.end() || !kit->is_string()) {
            sk_error("invalid response: %s", response.c_str());
            ret = TKERR_ILLEGAL_PKG;
            ctx->cb(ret, ctx->key, nullptr);
            return ret;
        }
        sk_assert(*kit == ctx->key);

        auto vit = object.find("Value");
        if (vit == object.end() || !vit->is_string()) {
            sk_error("invalid response: %s", response.c_str());
            ret = TKERR_ILLEGAL_PKG;
            ctx->cb(ret, ctx->key, nullptr);
            return ret;
        }

        std::string value = sk::base64_decode(*vit);
        ctx->cb(0, ctx->key, &value);
    } catch (...) {
        sk_fatal("exception caught, response: %s", response.c_str());
        ret = TKERR_ILLEGAL_PKG;
        ctx->cb(ret, ctx->key, nullptr);
        return ret;
    }

    return 0;
}

int consul::on_set(int ret, int status,
                   const std::string& response,
                   const void *cb_data, int cb_len) {
    consul_set_ctx *ctx = nullptr;
    assert_retval(cb_data && cb_len == sizeof(ctx), -1);
    ctx = (consul_set_ctx *) (*((uintptr_t *) cb_data));

    struct guard {
        guard(consul_set_ctx *ctx) { this->ctx = ctx; }
        ~guard() { delete ctx; }

        consul_set_ctx *ctx;
    } g(ctx);

    if (ret != 0) {
        sk_error("curl error<%d>.", ret);
        ctx->cb(ret, ctx->key, ctx->value);
        return ret;
    }

    sk_debug("status: %d, response: %s", status, response.c_str());

    if (status == 404) {
        ret = TKERR_NOTEXIST;
        ctx->cb(ret, ctx->key, ctx->value);
        return ret;
    }

    if (status == 400) {
        ret = TKERR_INVALID_ARG;
        ctx->cb(ret, ctx->key, ctx->value);
        return ret;
    }

    if (status != 200) {
        sk_error("unknown status code<%d>.", status);
        ret = TKERR_UNKNOWN;
        ctx->cb(ret, ctx->key, ctx->value);
        return ret;
    }

    try {
        auto object = nlohmann::json::parse(response);
        if (!object.is_boolean()) {
            sk_error("invalid response: %s", response.c_str());
            ret = TKERR_ILLEGAL_PKG;
            ctx->cb(ret, ctx->key, ctx->value);
            return ret;
        }

        if (!object) {
            sk_error("set returns false.");
            ret = TKERR_NOTMATCH;
            ctx->cb(ret, ctx->key, ctx->value);
            return ret;
        }

        ctx->cb(0, ctx->key, ctx->value);
    } catch (...) {
        sk_fatal("exception caught, response: %s", response.c_str());
        ret = TKERR_ILLEGAL_PKG;
        ctx->cb(ret, ctx->key, ctx->value);
        return ret;
    }

    return 0;
}

int consul::on_del(int ret, int status,
                   const std::string& response,
                   const void *cb_data, int cb_len) {
    consul_del_ctx *ctx = nullptr;
    assert_retval(cb_data && cb_len == sizeof(ctx), -1);
    ctx = (consul_del_ctx *) (*((uintptr_t *) cb_data));

    struct guard {
        guard(consul_del_ctx *ctx) { this->ctx = ctx; }
        ~guard() { delete ctx; }

        consul_del_ctx *ctx;
    } g(ctx);

    if (ret != 0) {
        sk_error("curl error<%d>.", ret);
        ctx->cb(ret, ctx->recursive, ctx->key);
        return ret;
    }

    sk_debug("status: %d, response: %s", status, response.c_str());

    if (status == 404) {
        ret = TKERR_NOTEXIST;
        ctx->cb(ret, ctx->recursive, ctx->key);
        return ret;
    }

    if (status == 400) {
        ret = TKERR_INVALID_ARG;
        ctx->cb(ret, ctx->recursive, ctx->key);
        return ret;
    }

    if (status != 200) {
        sk_error("unknown status code<%d>.", status);
        ret = TKERR_UNKNOWN;
        ctx->cb(ret, ctx->recursive, ctx->key);
        return ret;
    }

    try {
        auto object = nlohmann::json::parse(response);
        if (!object.is_boolean()) {
            sk_error("invalid response: %s", response.c_str());
            ret = TKERR_ILLEGAL_PKG;
            ctx->cb(ret, ctx->recursive, ctx->key);
            return ret;
        }

        if (!object) {
            sk_error("del returns false.");
            ret = TKERR_NOTMATCH;
            ctx->cb(ret, ctx->recursive, ctx->key);
            return ret;
        }

        ctx->cb(0, ctx->recursive, ctx->key);
    } catch (...) {
        sk_fatal("exception caught, response: %s", response.c_str());
        ret = TKERR_ILLEGAL_PKG;
        ctx->cb(ret, ctx->recursive, ctx->key);
        return ret;
    }

    return 0;
}
