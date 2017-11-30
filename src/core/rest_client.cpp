#include <signal.h>
#include <core/rest_client.h>
#include <utility/assert_helper.h>
#include <utility/string_helper.h>

#define DEFAULT_CONNECT_TIMEOUT_MS  15000 // 15 seconds
#define DEFAULT_TRANSFER_TIMEOUT_MS 30000 // 30 seconds

NS_BEGIN(sk)

rest_client::rest_client(uv_loop_t *loop, const std::string& host)
    : connect_timeout_ms_(DEFAULT_CONNECT_TIMEOUT_MS),
      transfer_timeout_ms_(DEFAULT_TRANSFER_TIMEOUT_MS),
      mh_(nullptr), loop_(loop), host_(host),
      timer_(loop, std::bind(&rest_client::on_timeout, this, std::placeholders::_1)) {
    curl_global_init(CURL_GLOBAL_ALL);
    mh_ = curl_multi_init();

    curl_multi_setopt(mh_, CURLMOPT_TIMERDATA, this);
    curl_multi_setopt(mh_, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(mh_, CURLMOPT_TIMERFUNCTION, rest_client::start_timer);
    curl_multi_setopt(mh_, CURLMOPT_SOCKETFUNCTION, rest_client::handle_socket);

    signal(SIGPIPE, SIG_IGN);
}

rest_client::~rest_client() {
    sk_assert(handles_.empty());
    sk_assert(contexts_.empty());
    curl_multi_cleanup(mh_);
    curl_global_cleanup();
}

void rest_client::stop() {
    while (!handles_.empty()) {
        auto it = handles_.begin();
        http_context *ctx = nullptr;
        curl_easy_getinfo(*it, CURLINFO_PRIVATE, &ctx);

        curl_multi_remove_handle(mh_, *it);
        curl_easy_cleanup(*it);
        handles_.erase(it);
        delete ctx;
    }

    if (!timer_.stopped()) timer_.stop();
}

int rest_client::get(const char *uri, const fn_on_http_response& fn,
                     const string_map *parameters, const string_map *headers) {
    CURL *handle = create_handle(uri, fn, parameters, headers);

    CURLMcode rc = curl_multi_add_handle(mh_, handle);
    sk_assert(rc == CURLM_OK);

    sk_assert(handles_.find(handle) == handles_.end());
    handles_.insert(handle);
    return 0;
}

int rest_client::del(const char *uri, const fn_on_http_response& fn,
                     const string_map *parameters, const string_map *headers) {
    CURL *handle = create_handle(uri, fn, parameters, headers);
    curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "DELETE");

    CURLMcode rc = curl_multi_add_handle(mh_, handle);
    sk_assert(rc == CURLM_OK);

    sk_assert(handles_.find(handle) == handles_.end());
    handles_.insert(handle);
    return 0;
}

int rest_client::put(const char *uri, const fn_on_http_response& fn,
                     const char *payload, size_t payload_len,
                     const string_map *parameters, const string_map *headers) {
    CURL *handle = create_handle(uri, fn, parameters, headers);
    curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, payload_len);
    curl_easy_setopt(handle, CURLOPT_COPYPOSTFIELDS, payload);

    CURLMcode rc = curl_multi_add_handle(mh_, handle);
    sk_assert(rc == CURLM_OK);

    sk_assert(handles_.find(handle) == handles_.end());
    handles_.insert(handle);
    return 0;
}

int rest_client::post(const char *uri, const fn_on_http_response& fn,
                      const char *payload, size_t payload_len,
                      const string_map *parameters, const string_map *headers) {
    CURL *handle = create_handle(uri, fn, parameters, headers);
    curl_easy_setopt(handle, CURLOPT_POST, 1);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, payload_len);
    curl_easy_setopt(handle, CURLOPT_COPYPOSTFIELDS, payload);

    CURLMcode rc = curl_multi_add_handle(mh_, handle);
    sk_assert(rc == CURLM_OK);

    sk_assert(handles_.find(handle) == handles_.end());
    handles_.insert(handle);
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

void rest_client::on_timeout(heap_timer *timer) {
    sk_assert(&timer_ == timer);
    int running = 0;
    curl_multi_socket_action(mh_, CURL_SOCKET_TIMEOUT, 0, &running);
    check_multi_info();
}

void rest_client::check_multi_info() {
    int pending = 0;
    CURLMsg *msg = nullptr;
    while ((msg = curl_multi_info_read(mh_, &pending))) {
        if (msg->msg == CURLMSG_DONE) {
            long status = 0;
            http_context *ctx = nullptr;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &ctx);
            curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &status);

            ctx->cb(msg->data.result,
                    static_cast<int>(status),
                    ctx->rsp_headers, ctx->rsp_body);

            handles_.erase(msg->easy_handle);
            curl_multi_remove_handle(mh_, msg->easy_handle);
            curl_easy_cleanup(msg->easy_handle);
            delete ctx;
        }
    }
}

CURL *rest_client::create_handle(const char *uri,
                                 const fn_on_http_response& fn,
                                 const string_map *parameters,
                                 const string_map *headers) {
    CURL *handle = curl_easy_init();
    http_context *ctx = new http_context(headers, fn);
    std::string url = make_url(uri, parameters);
    sk_debug("request url: %s", url.c_str());

    if (ctx->req_headers)
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, ctx->req_headers);

    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_PRIVATE, ctx);

    // curl_easy_setopt(h, CURLOPT_DNS_CACHE_TIMEOUT, 0);
    // curl_easy_setopt(h, CURLOPT_FORBID_REUSE, 0);
    if (connect_timeout_ms_ > 0)
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_ms_);
    if (transfer_timeout_ms_ > 0)
        curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, transfer_timeout_ms_);

#ifndef NDEBUG
    curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION, rest_client::on_log);
#endif

    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, rest_client::on_header);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, ctx);

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, rest_client::on_body);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, ctx);

    return handle;
}

int rest_client::handle_socket(CURL *h, curl_socket_t sockfd,
                               int action, void *userp, void *socketp) {
    unused_parameter(h);
    rest_client *client = static_cast<rest_client*>(userp);
    switch (action) {
    case CURL_POLL_IN:
    case CURL_POLL_OUT:
    case CURL_POLL_INOUT: {
        int events = 0;
        poll_context *ctx = static_cast<poll_context*>(socketp);
        if (!ctx) {
            ctx = new poll_context(client->loop_, sockfd, client);
            sk_assert(client->contexts_.find(ctx) == client->contexts_.end());
            client->contexts_.insert(ctx);
        }

        curl_multi_assign(client->mh_, sockfd, ctx);

        if (action != CURL_POLL_IN)
            events |= UV_WRITABLE;
        if (action != CURL_POLL_OUT)
            events |= UV_READABLE;

        int ret = uv_poll_start(&ctx->poll.poll, events, rest_client::on_event);
        sk_assert(ret == 0);
        break;
    }
    case CURL_POLL_REMOVE: {
        if (socketp) {
            curl_multi_assign(client->mh_, sockfd, nullptr);
            poll_context *ctx = static_cast<poll_context*>(socketp);
            client->contexts_.erase(ctx);
            uv_poll_stop(&ctx->poll.poll);
            uv_close(&ctx->poll.handle, rest_client::on_close);
        }
        break;
    }
    default:
        abort();
    }

    return 0;
}

int rest_client::start_timer(CURLM *, long timeout_ms, void *userp) {
    rest_client *client = static_cast<rest_client*>(userp);
    if (timeout_ms < 0) {
        client->timer_.stop();
    } else {
        client->timer_.stop();
        client->timer_.start_once(static_cast<u64>(timeout_ms));
    }

    return 0;
}

void rest_client::on_event(uv_poll_t *handle, int status, int events) {
    poll_context *ctx = static_cast<poll_context*>(handle->data);
    sk_assert(&ctx->poll.poll == handle);
    sk_assert(ctx->client->contexts_.find(ctx) != ctx->client->contexts_.end());

    if (status != 0) sk_error("on_event: %s", uv_err_name(status));

    int flags = 0;
    if (events & UV_READABLE)
        flags |= CURL_CSELECT_IN;
    if (events & UV_WRITABLE)
        flags |= CURL_CSELECT_OUT;

    int running = 0;
    curl_multi_socket_action(ctx->client->mh_, ctx->sockfd, flags, &running);
    ctx->client->check_multi_info();
}

void rest_client::on_close(uv_handle_t *handle) {
    poll_context *ctx = static_cast<poll_context*>(handle->data);
    sk_assert(&ctx->poll.handle == handle);
    sk_assert(ctx->client->contexts_.find(ctx) == ctx->client->contexts_.end());

    delete ctx;
}

int rest_client::on_log(CURL *, curl_infotype type, char *data, size_t size, void *) {
    const char *text = nullptr;

    switch (type) {
    case CURLINFO_TEXT:
        sk_trace("== info: %s", data);
#if GCC_VERSION >= 70000
        __attribute__ ((fallthrough));
#endif
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

size_t rest_client::on_header(char *ptr, size_t size, size_t nmemb, void *userp) {
    http_context *ctx = static_cast<http_context*>(userp);
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

    ctx->rsp_headers[k] = v;
    return total;
}

size_t rest_client::on_body(char *ptr, size_t size, size_t nmemb, void *userp) {
    http_context *ctx = static_cast<http_context*>(userp);
    size_t total = size * nmemb;
    ctx->rsp_body.append(ptr, total);
    return total;
}

NS_END(sk)
