#ifndef CONSUL_H
#define CONSUL_H

#include <list>
#include <string>
#include "rest_client.h"

class consul {
public:
    typedef std::vector<std::string> string_vector;
    typedef std::map<std::string, std::string> string_map;
    typedef std::function<void(int, const string_map*)> fn_on_get_all;
    typedef std::function<void(int, const std::string&, const std::string*)> fn_on_get;
    typedef std::function<void(int, const std::string&, const std::string&)> fn_on_set;
    typedef std::function<void(int, bool, const std::string&)> fn_on_del;

    int init(const string_vector& addr_list);

    int get_all(const std::string& prefix, fn_on_get_all cb);

    int get(const std::string& key, fn_on_get cb);

    int set(const std::string& key, const std::string& value, fn_on_set cb);

    int del(const std::string& key, bool recursive, fn_on_del cb);

    void run();

private:
    static int on_get_all(int ret, int status,
                          const std::string& response,
                          const void *cb_data, int cb_len);

    static int on_get(int ret, int status,
                      const std::string& response,
                      const void *cb_data, int cb_len);

    static int on_set(int ret, int status,
                      const std::string& response,
                      const void *cb_data, int cb_len);

    static int on_del(int ret, int status,
                      const std::string& response,
                      const void *cb_data, int cb_len);

private:
    std::shared_ptr<rest_client> master_client_;
    std::list<std::shared_ptr<rest_client>> backup_clients_;
};

#endif // CONSUL_H
