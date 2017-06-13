#ifndef CONSUL_CLIENT_H
#define CONSUL_CLIENT_H

#include <list>
#include <net/reactor.h>
#include <utility/types.h>
#include <utility/rest_client.h>

class consul_set_context;
class consul_del_context;

NS_BEGIN(sk)

class consul_client {
public:
    using string_vector = std::vector<std::string>;
    using string_map = std::map<std::string, std::string>;
    using fn_on_watch = std::function<void(int, int, const string_map*)>;
    using fn_on_set = std::function<void(int, const std::string&, const std::string&)>;
    using fn_on_del = std::function<void(int, bool, const std::string&)>;

    int init(net::reactor *r, const string_vector& addr_list);

    int watch(const std::string& prefix, int index, const fn_on_watch& fn);
    int set(const std::string& key, const std::string& value, const fn_on_set& fn);
    int del(const std::string& key, bool recursive, const fn_on_del& fn);

private:
    void on_watch(int ret, int status,
                  const string_map& headers,
                  const std::string& body,
                  const fn_on_watch& fn);

    void on_set(consul_set_context *ctx,
                int ret, int status,
                const string_map& headers,
                const std::string& body,
                const fn_on_set& fn);

    void on_del(consul_del_context *ctx,
                int ret, int status,
                const string_map& headers,
                const std::string& body,
                const fn_on_del& fn);

private:
    // TODO: add master/backup switch logic
    std::shared_ptr<rest_client> master_;
    std::list<std::shared_ptr<rest_client>> backup_;
};

NS_END(sk)

#endif // CONSUL_CLIENT_H
