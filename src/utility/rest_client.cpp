#include <signal.h>
#include <utility/rest_client.h>
#include <utility/assert_helper.h>
#include <utility/string_helper.h>

#define DEFAULT_CONNECT_TIMEOUT_MS  15000 // 15 seconds
#define DEFAULT_TRANSFER_TIMEOUT_MS 30000 // 30 seconds

NS_BEGIN(sk)

rest_client::context::context()
    : handle_(curl_easy_init()), outgoing_headers_(nullptr) {}

rest_client::context::~context() {
    if (handle_) {
        curl_easy_cleanup(handle_);
        handle_ = nullptr;
    }

    if (outgoing_headers_) {
        curl_slist_free_all(outgoing_headers_);
        outgoing_headers_ = nullptr;
    }
}

void rest_client::context::init(const std::string& url,
                                const string_map *headers,
                                long connect_timeout_ms,
                                long transfer_timeout_ms,
                                const fn_on_response& fn) {
    sk_assert(!outgoing_headers_);
    this->fn_on_rsp_ = fn;

    if (headers) {
        for (const auto& it : *headers) {
            std::string str(it.first);
            str.append(": ").append(it.second);
            outgoing_headers_ = curl_slist_append(outgoing_headers_, str.c_str());
        }

        if (outgoing_headers_)
            curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, outgoing_headers_);
    }

    curl_easy_setopt(handle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle_, CURLOPT_PRIVATE, this);

    // curl_easy_setopt(h, CURLOPT_DNS_CACHE_TIMEOUT, 0);
    // curl_easy_setopt(h, CURLOPT_FORBID_REUSE, 0);
    if (connect_timeout_ms > 0)
        curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_ms);
    if (transfer_timeout_ms > 0)
        curl_easy_setopt(handle_, CURLOPT_TIMEOUT_MS, transfer_timeout_ms);

#ifndef NDEBUG
    curl_easy_setopt(handle_, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(handle_, CURLOPT_DEBUGFUNCTION, context::on_log);
#endif

    curl_easy_setopt(handle_, CURLOPT_HEADERFUNCTION, context::on_header);
    curl_easy_setopt(handle_, CURLOPT_HEADERDATA, this);

    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, context::on_body);
    curl_easy_setopt(handle_, CURLOPT_WRITEDATA, this);
}

void rest_client::context::start(rest_client *client, int sockfd, net::reactor *r) {
    if (handler_) {
        sk_assert(handler_->fd() == sockfd);
        return;
    }

    handler_ = net::handler_ptr(new net::handler(r, sockfd));
    handler_->set_read_callback(std::bind(&rest_client::on_response, client, this));
    handler_->set_write_callback(std::bind(&rest_client::on_request, client, this));
}

void rest_client::context::on_response(int rc, int status_code) {
    fn_on_rsp_(rc, status_code, incoming_headers_, response_);
}

void rest_client::context::reset() {
    curl_easy_reset(handle_);
    if (outgoing_headers_) {
        curl_slist_free_all(outgoing_headers_);
        outgoing_headers_ = nullptr;
    }

    handler_.reset();
    response_.clear();
    incoming_headers_.clear();
    fn_on_rsp_ = nullptr;
}

int rest_client::context::on_log(CURL *, curl_infotype type,
                                 char *data, size_t size, void *) {
    const char *text = nullptr;

    switch (type) {
    case CURLINFO_TEXT:
        sk_trace("== info: %s", data);
    default: /* in case a new one is introduced to shock us */
        return 0;

    case CURLINFO_HEADER_OUT:
        text = "=> send header";
        break;
    case CURLINFO_DATA_OUT:
        text = "=> send data";
        break;
    case CURLINFO_SSL_DATA_OUT:
        text = "=> send ssl data";
        break;
    case CURLINFO_HEADER_IN:
        text = "<= recv header";
        break;
    case CURLINFO_DATA_IN:
        text = "<= recv data";
        break;
    case CURLINFO_SSL_DATA_IN:
        text = "<= recv ssl data";
        break;
    }

    sk_trace("%s, %10.10ld bytes (0x%8.8lx)\n%s", text, size, size, data);
    return 0;
}

size_t rest_client::context::on_header(char *ptr, size_t size, size_t nmemb, void *userp) {
    context *ctx = static_cast<context*>(userp);
    size_t total = size * nmemb;

    std::string str(ptr, total);
    sk::trim_string(str);

    if (str.empty()) return total;
    if (sk::is_prefix("HTTP/1.", str)) return total;

    auto pos = str.find(':');
    if (pos == str.npos) {
        sk_error("invalid header line: %s", str.c_str());
        return total;
    }

    std::string k = str.substr(0, pos);
    std::string v = str.substr(pos + 1);
    sk::trim_string(k);
    sk::trim_string(v);

    ctx->incoming_headers_[k] = v;
    return total;
}

size_t rest_client::context::on_body(char *ptr, size_t size, size_t nmemb, void *userp) {
    context *ctx = static_cast<context*>(userp);
    size_t total = size * nmemb;
    ctx->response_.append(ptr, total);
    return total;
}


rest_client::rest_client(net::reactor *r, const std::string& host, size_t max_cache_count)
    : connect_timeout_ms_(DEFAULT_CONNECT_TIMEOUT_MS),
      transfer_timeout_ms_(DEFAULT_TRANSFER_TIMEOUT_MS),
      reactor_(r), host_(host), max_cache_count_(max_cache_count) {
    curl_global_init(CURL_GLOBAL_ALL);
    mh_ = curl_multi_init();

    curl_multi_setopt(mh_, CURLMOPT_TIMERDATA, this);
    curl_multi_setopt(mh_, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(mh_, CURLMOPT_TIMERFUNCTION, rest_client::handle_timer);
    curl_multi_setopt(mh_, CURLMOPT_SOCKETFUNCTION, rest_client::handle_socket);

    signal(SIGPIPE, SIG_IGN);
}

rest_client::~rest_client() {
    for (const auto& it : free_contexts_)
        delete it;

    for (const auto& it : busy_contexts_)
        delete it;

    free_contexts_.clear();
    busy_contexts_.clear();

    if (timer_) {
        timer_->cancel();
        timer_.reset();
    }

    curl_multi_cleanup(mh_);
    curl_global_cleanup();
}

int rest_client::get(const char *uri, const fn_on_response& fn,
                     const string_map *parameters, const string_map *headers) {
    context *ctx = fetch_context(uri, parameters, headers, fn);
    assert_retval(ctx, -1);

    CURLMcode rc = curl_multi_add_handle(mh_, ctx->handle());
    if (rc != CURLM_OK) {
        sk_error("failed to add handle, ret<%d>.", rc);
        release_context(ctx);
        return -1;
    }

    busy_contexts_.insert(ctx);
    return 0;
}

int rest_client::del(const char *uri, const fn_on_response& fn,
                     const string_map *parameters, const string_map *headers) {
    context *ctx = fetch_context(uri, parameters, headers, fn);
    assert_retval(ctx, -1);

    curl_easy_setopt(ctx->handle(), CURLOPT_CUSTOMREQUEST, "DELETE");
    CURLMcode rc = curl_multi_add_handle(mh_, ctx->handle());
    if (rc != CURLM_OK) {
        sk_error("failed to add handle, ret<%d>.", rc);
        release_context(ctx);
        return -1;
    }

    busy_contexts_.insert(ctx);
    return 0;
}

int rest_client::put(const char *uri, const fn_on_response& fn,
                     const char *payload, size_t payload_len,
                     const string_map *parameters, const string_map *headers) {
    context *ctx = fetch_context(uri, parameters, headers, fn);
    assert_retval(ctx, -1);

    curl_easy_setopt(ctx->handle(), CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(ctx->handle(), CURLOPT_POSTFIELDSIZE, payload_len);
    curl_easy_setopt(ctx->handle(), CURLOPT_COPYPOSTFIELDS, payload);
    CURLMcode rc = curl_multi_add_handle(mh_, ctx->handle());
    if (rc != CURLM_OK) {
        sk_error("failed to add handle, ret<%d>.", rc);
        release_context(ctx);
        return -1;
    }

    busy_contexts_.insert(ctx);
    return 0;
}

int rest_client::post(const char *uri, const fn_on_response& fn,
                      const char *payload, size_t payload_len,
                      const string_map *parameters, const string_map *headers) {
    context *ctx = fetch_context(uri, parameters, headers, fn);
    assert_retval(ctx, -1);

    curl_easy_setopt(ctx->handle(), CURLOPT_POST, 1);
    curl_easy_setopt(ctx->handle(), CURLOPT_POSTFIELDSIZE, payload_len);
    curl_easy_setopt(ctx->handle(), CURLOPT_COPYPOSTFIELDS, payload);
    CURLMcode rc = curl_multi_add_handle(mh_, ctx->handle());
    if (rc != CURLM_OK) {
        sk_error("failed to add handle, ret<%d>.", rc);
        release_context(ctx);
        return -1;
    }

    busy_contexts_.insert(ctx);
    return 0;
}

std::string rest_client::make_url(const char *uri, const string_map *parameters) {
    std::string url(host_);
    if (uri) url.append(uri);

    if (parameters) {
        std::string query("?");
        for (const auto& pair : *parameters) {
            if (!pair.first.empty()) {
                query.append(pair.first);
                if (!pair.second.empty())
                    query.append(1, '=').append(pair.second);
                query.append(1, '&');
            }
        }
        if (!query.empty() && query.back() == '&')
            query.resize(query.size() - 1);

        url.append(query);
    }

    return url;
}

void rest_client::try_read_response() {
    int pending = 0;
    CURLMsg *msg = nullptr;
    while ((msg = curl_multi_info_read(mh_, &pending))) {
        if (msg->msg == CURLMSG_DONE) {
            context *ctx = nullptr;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &ctx);
            sk_assert(ctx && ctx->handle() == msg->easy_handle);

            long status = 0;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &status);
            sk_debug("response, ret: %d, status: %ld", msg->data.result, status);

            ctx->on_response(msg->data.result, static_cast<int>(status));

            curl_multi_remove_handle(mh_, msg->easy_handle);
            busy_contexts_.erase(ctx);
            release_context(ctx);
        }
    }
}

void rest_client::on_request(context *ctx) {
    sk_assert(busy_contexts_.find(ctx) != busy_contexts_.end());

    int running = 0;
    int flags = 0;

    flags |= CURL_CSELECT_OUT;

    curl_multi_socket_action(mh_, ctx->socket(), flags, &running);
    try_read_response();
}

void rest_client::on_response(context *ctx) {
    sk_assert(busy_contexts_.find(ctx) != busy_contexts_.end());

    int running = 0;
    int flags = 0;

    flags |= CURL_CSELECT_IN;

    curl_multi_socket_action(mh_, ctx->socket(), flags, &running);
    try_read_response();
}

int rest_client::on_socket(CURL *h, curl_socket_t sockfd, int action, context *ctx) {
    switch (action) {
    case CURL_POLL_IN:
    case CURL_POLL_OUT:
    case CURL_POLL_INOUT: {
        if (!ctx) {
            curl_easy_getinfo(h, CURLINFO_PRIVATE, &ctx);
            sk_assert(ctx && ctx->handle() == h);
        }

        ctx->start(this, sockfd, reactor_);
        curl_multi_assign(mh_, sockfd, ctx);

        // this function will be called multiple times, so there might
        // exist read/write events, disable them all first, and then
        // add desired event(s)
        ctx->disable_all();

        if (action != CURL_POLL_IN)  ctx->enable_writing();
        if (action != CURL_POLL_OUT) ctx->enable_reading();

        break;
    }
    case CURL_POLL_REMOVE: {
        if (ctx) {
            ctx->disable_all();
            curl_multi_assign(mh_, sockfd, nullptr);
            // do NOT release here, otherwise, the msg done event
            // will not be received in try_read_response() function
            // release_context(ctx);
        }
        break;
    }
    default:
        abort();
    }

    return 0;
}

int rest_client::handle_socket(CURL *h, curl_socket_t sockfd,
                               int action, void *userp, void *socketp) {
    rest_client *client = static_cast<rest_client*>(userp);
    context *ctx = static_cast<context*>(socketp);
    return client->on_socket(h, sockfd, action, ctx);
}

void rest_client::on_timeout(const time::heap_timer_ptr& timer) {
    sk_assert(timer == timer_);
    timer_.reset();
    int running = 0;
    curl_multi_socket_action(mh_, CURL_SOCKET_TIMEOUT, 0, &running);
    try_read_response();
}

int rest_client::on_timer(long timeout_ms) {
    if (timeout_ms < 0) {
        if (timer_) timer_->cancel();
        else sk_warn("no timer.");
    } else if (timeout_ms == 0) {
        if (timer_) timer_->cancel();
        on_timeout(timer_);
    } else {
        // TODO: improve the precision here, libcurl uses ms not sec
        u32 sec = static_cast<u32>(timeout_ms * 1.0 / 1000);
        if (sec <= 0) sec = 1;

        if (timer_) timer_->cancel();
        timer_ = sk::time::add_single_timer(sec, std::bind(&rest_client::on_timeout, this, std::placeholders::_1));
    }

    return 0;
}

int rest_client::handle_timer(CURLM *mh, long timeout_ms, void *userp) {
    rest_client *client = static_cast<rest_client*>(userp);
    sk_assert(client->mh_ == mh);
    return client->on_timer(timeout_ms);
}

rest_client::context *rest_client::fetch_context(const char *uri,
                                                 const string_map *parameters,
                                                 const string_map *headers,
                                                 const fn_on_response& fn) {
    context *ctx = nullptr;

    do {
        if (!free_contexts_.empty()) {
            ctx = free_contexts_.back();
            free_contexts_.pop_back();
            break;
        }

        ctx = new context();
        if (ctx) break;

        sk_error("OOM!");
        return nullptr;
    } while (0);

    std::string url = make_url(uri, parameters);
    sk_debug("entire url: %s", url.c_str());

    ctx->init(url, headers, connect_timeout_ms_, transfer_timeout_ms_, fn);
    return ctx;
}

void rest_client::release_context(context *ctx) {
    if (free_contexts_.size() >= max_cache_count_) {
        delete ctx;
        return;
    }

    ctx->reset();
    free_contexts_.push_back(ctx);
}

NS_END(sk)
