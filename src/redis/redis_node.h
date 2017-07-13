#ifndef REDIS_NODE_H
#define REDIS_NODE_H

#include <string>
#include <memory>
#include <utility/types.h>

NS_BEGIN(sk)

class redis_node {
public:
    redis_node(const std::string& host, int port)
        : port_(port), host_(host) {
        id_.reserve(host_.length() + 1 + 6); // host + : + port
        id_.append(host_).append(1, ':').append(std::to_string(port));
    }

    int port() const { return port_; }
    const std::string& host() const { return host_; }
    const std::string& id()   const { return id_; }
    const char *c_str() const { return id_.c_str(); }

    bool operator<(const redis_node& that) const {
        return this->id() < that.id();
    }

private:
    int port_;
    std::string host_;
    std::string id_;
};
using redis_node_ptr = std::unique_ptr<redis_node>;

NS_END(sk)

#endif // REDIS_NODE_H
