#ifndef REST_CLIENT_H
#define REST_CLIENT_H

#include <map>
#include <set>
#include <vector>
#include <functional>
#include <curl/curl.h>
#include <net/handler.h>
#include <utility/types.h>
#include <time/heap_timer.h>

NS_BEGIN(sk)

class rest_client {
public:
    using string_map = std::map<std::string, std::string>;
    using fn_on_response = std::function<void(int, int, const string_map&, const std::string&)>;

    rest_client(net::reactor *r, const std::string& host, size_t max_cache_count);
    ~rest_client();

    int get(const char *uri, const fn_on_response& fn,
            const string_map *parameters, const string_map *headers);

private:
    class context {
    public:
        context();
        ~context();

        void init(const std::string& url,
                  const string_map *headers,
                  const fn_on_response& fn);
        void start(rest_client *client, int sockfd, net::reactor *r);
        void on_response(int rc, int status_code);
        void reset();

        CURL *handle() const { return handle_; }
        int socket() const { return handler_->fd(); }

        void enable_reading() { handler_->enable_reading(); }
        void enable_writing() { handler_->enable_writing(); }
        void disable_all()    { handler_->disable_all(); }

    private:
        static int on_log(CURL *, curl_infotype type, char *data, size_t size, void *);
        static size_t on_header(char *ptr, size_t size, size_t nmemb, void *userp);
        static size_t on_body(char *ptr, size_t size, size_t nmemb, void *userp);

    private:
        CURL *handle_;
        curl_slist *outgoing_headers_;
        net::handler_ptr handler_;
        std::string response_;
        string_map incoming_headers_;
        fn_on_response fn_on_rsp_;
    };

private:
    std::string make_url(const char *uri, const string_map *parameters);
    void try_read_response();

    context *fetch_context(const std::string& url,
                           const string_map *headers,
                           const fn_on_response& fn);
    void release_context(context *ctx);

private:
    void on_request(context *ctx);
    void on_response(context *ctx);

    int on_socket(CURL *h, curl_socket_t sockfd, int action, context *ctx);
    static int handle_socket(CURL *h, curl_socket_t sockfd,
                             int action, void *userp, void *socketp);

    void on_timeout(const time::heap_timer_ptr&);
    int on_timer(long timeout_ms);
    static int handle_timer(CURLM *mh, long timeout_ms, void *userp);

private:
    net::reactor *reactor_;
    std::string host_;
    size_t max_cache_count_;
    CURLM *mh_;
    time::heap_timer_ptr timer_;

    std::vector<context*> free_contexts_;
    std::set<context*>    busy_contexts_;
};

NS_END(sk)

#endif // REST_CLIENT_H
