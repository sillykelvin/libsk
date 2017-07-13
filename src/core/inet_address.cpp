#include <string.h>
#include <arpa/inet.h>
#include <core/inet_address.h>
#include <utility/assert_helper.h>

NS_BEGIN(sk)

inet_address::inet_address(u16 port) {
    memset(&addr_, 0x00, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = INADDR_ANY;

    set_string();
}

inet_address::inet_address(const std::string& ip, u16 port) {
    memset(&addr_, 0x00, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    int ret = inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
    sk_assert(ret == 1);

    set_string();
}

inet_address::inet_address(const sockaddr_in& addr) {
    memcpy(&addr_, &addr, sizeof(addr_));

    set_string();
}

void inet_address::set_string() {
    char addr[64] = {0};
    char host[32] = "INVALID";

    u16 port = ntohs(addr_.sin_port);
    inet_ntop(AF_INET, &addr_.sin_addr, host, sizeof(host));
    snprintf(addr, sizeof(addr), "%s:%u", host, port);

    host_ = host;
    full_ = addr;
}

NS_END(sk)
