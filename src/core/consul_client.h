#ifndef CONSUL_CLIENT_H
#define CONSUL_CLIENT_H

#include <list>
#include <core/rest_client.h>

class consul_set_context;
class consul_del_context;

NS_BEGIN(sk)

class consul_client {
public:
    int init(uv_loop_t *loop, const string_vector& addr_list);
    void stop();

    int watch(const std::string& prefix, int index, const fn_on_consul_watch& fn);
    int set(const std::string& key, const std::string& value, const fn_on_consul_set& fn);
    int del(const std::string& key, bool recursive, const fn_on_consul_del& fn);

private:
    void on_watch(int ret, int status,
                  const string_map& headers,
                  const std::string& body,
                  const fn_on_consul_watch& fn);

    void on_set(consul_set_context *ctx,
                int ret, int status,
                const string_map& headers,
                const std::string& body,
                const fn_on_consul_set& fn);

    void on_del(consul_del_context *ctx,
                int ret, int status,
                const string_map& headers,
                const std::string& body,
                const fn_on_consul_del& fn);

private:
    // TODO: add master/backup switch logic
    std::shared_ptr<rest_client> master_;
    std::list<std::shared_ptr<rest_client>> backup_;
};

NS_END(sk)

#endif // CONSUL_CLIENT_H
