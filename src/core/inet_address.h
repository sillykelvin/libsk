#ifndef INET_ADDRESS_H
#define INET_ADDRESS_H

#include <string>
#include <netdb.h>
#include <utility/types.h>

NS_BEGIN(sk)

class inet_address {
public:
    explicit inet_address(u16 port);
    inet_address(const std::string& ip, u16 port);
    explicit inet_address(const struct sockaddr_in& addr);

    const struct sockaddr *address() const {
        return reinterpret_cast<const struct sockaddr*>(&addr_);
    }

    const std::string& as_string() const { return full_; }
    const std::string& host() const { return host_; }
    u16 port() const { return ntohs(addr_.sin_port); }

private:
    void set_string();

private:
    struct sockaddr_in addr_;
    std::string host_; // host only
    std::string full_; // full address presented as host:port
};

NS_END(sk)

#endif // INET_ADDRESS_H
