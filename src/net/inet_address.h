#ifndef INET_ADDRESS_H
#define INET_ADDRESS_H

#include <string>
#include <netdb.h>
#include "utility/types.h"

NS_BEGIN(sk)

class inet_address {
public:
    explicit inet_address(u16 port);
    inet_address(const std::string& ip, u16 port);
    inet_address(const struct sockaddr_in& addr);

    struct sockaddr *address() {
        return cast_ptr(sockaddr, &addr_);
    }

    const struct sockaddr *address() const {
        return reinterpret_cast<const struct sockaddr*>(&addr_);
    }

    socklen_t length() const {
        return sizeof(addr_);
    }

    std::string to_string() const;

private:
    struct sockaddr_in addr_;
};

NS_END(sk)

#endif // INET_ADDRESS_H
