#ifndef REST_CLIENT_H
#define REST_CLIENT_H

#include <uv.h>
#include <vector>
#include <curl/curl.h>
#include <unordered_set>
#include <core/callback.h>
#include <core/heap_timer.h>

NS_BEGIN(sk)

class rest_client {
public:
    rest_client(uv_loop_t *loop, const std::string& host);
    ~rest_client();

    void stop();

    /*
     * set connecting timeout value in millisecond, the default
     * value is 15000 (15 seconds), if it's set to 0, libcurl
     * will use its default value (300 seconds) internally
     */
    void set_connect_timeout(long timeout_ms) {
        if (timeout_ms >= 0) connect_timeout_ms_ = timeout_ms;
    }

    /*
     * set transferring timeout value in millisecond, 0 means
     * never timeout, the default value is 30000 (30 seconds)
     */
    void set_transfer_timeout(long timeout_ms) {
        if (timeout_ms >= 0) transfer_timeout_ms_ = timeout_ms;
    }

    int get(const char *uri, const fn_on_http_response& fn,
            const string_map *parameters, const string_map *headers);

    int del(const char *uri, const fn_on_http_response& fn,
            const string_map *parameters, const string_map *headers);

    int put(const char *uri, const fn_on_http_response& fn,
            const char *payload, size_t payload_len,
            const string_map *parameters, const string_map *headers);

    int post(const char *uri, const fn_on_http_response& fn,
             const char *payload, size_t payload_len,
             const string_map *parameters, const string_map *headers);

private:
    struct http_context {
        fn_on_http_response cb;  // response callback
        std::string rsp_body;    // response body
        string_map rsp_headers;  // response headers
        curl_slist *req_headers; // request headers

        http_context(const string_map *headers,
                     const fn_on_http_response& cb) {
            this->cb = cb;
            this->req_headers = nullptr;

            if (!headers) return;

            char tmp[256];
            for (const auto& it : *headers) {
                snprintf(tmp, sizeof(tmp), "%s: %s", it.first.c_str(), it.second.c_str());
                req_headers = curl_slist_append(req_headers, tmp);
            }
        }

        ~http_context() {
            if (req_headers) {
                curl_slist_free_all(req_headers);
                req_headers = nullptr;
            }
        }
    };

    struct poll_context {
        int sockfd;
        union {
            uv_handle_t handle;
            uv_poll_t poll;
        } poll;
        rest_client *client;

        poll_context(uv_loop_t *loop, int sockfd, rest_client *client) {
            this->sockfd = sockfd;
            this->client = client;

            uv_poll_init_socket(loop, &poll.poll, sockfd);
            poll.poll.data = this;
        }
    };

private:
    std::string make_url(const char *uri, const string_map *parameters);
    void on_timeout(heap_timer *timer);
    void check_multi_info();
    CURL *create_handle(const char *uri, const fn_on_http_response& fn,
                        const string_map *parameters, const string_map *headers);

private:
    static int handle_socket(CURL *h, curl_socket_t sockfd,
                             int action, void *userp, void *socketp);
    static int start_timer(CURLM *mh, long timeout_ms, void *userp);

    static void on_event(uv_poll_t *handle, int status, int events);
    static void on_close(uv_handle_t *handle);
    static int on_log(CURL *, curl_infotype type, char *data, size_t size, void *);
    static size_t on_header(char *ptr, size_t size, size_t nmemb, void *userp);
    static size_t on_body(char *ptr, size_t size, size_t nmemb, void *userp);

private:
    long connect_timeout_ms_;
    long transfer_timeout_ms_;
    CURLM *mh_;
    uv_loop_t *loop_;
    std::string host_;
    heap_timer timer_;
    std::unordered_set<CURL*> handles_;
    std::unordered_set<poll_context*> contexts_;
};

NS_END(sk)

#endif // REST_CLIENT_H
