#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "socket.h"
#include "utility/math_helper.h"
#include "utility/assert_helper.h"

NS_BEGIN(sk)

socket_ptr socket::create() {
    return socket_ptr(new socket());
}

socket::~socket() {
    sk_trace("~socket(%d)", fd_);
    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }
}

int socket::connect(const inet_address& addr) {
    int ret = 0;
    do {
        errno = 0;
        ret = ::connect(fd_, addr.address(), addr.length());
    } while (ret == -1 && errno == EINTR);

    // the socket is non-blocking, it's ok for
    // connect() to return an EINPROGRESS error
    if (ret == -1 && errno != EINPROGRESS) {
        ::close(fd_);
        fd_ = -1;
        return ret;
    }

    return 0;

    // struct addrinfo hints;
    // memset(&hints, 0x00, sizeof(hints));
    // hints.ai_family = AF_UNSPEC;
    // hints.ai_socktype = SOCK_STREAM;

    // struct addrinfo *server = nullptr;
    // std::string port_str = std::to_string(port);
    // int ret = getaddrinfo(addr.c_str(), port_str.c_str(), &hints, &server);
    // if (ret != 0) return ret;

    // for (struct addrinfo *p = server; p != nullptr; p = p->ai_next) {
    //     fd_ = make_socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    //     if (fd_ == -1) continue;

    //     do {
    //         errno = 0;
    //         ret = ::connect(fd_, p->ai_addr, p->ai_addrlen);
    //     } while (ret == -1 && errno == EINTR);

    //     // the socket is non-blocking, it's ok for
    //     // connect() to return an EINPROGRESS error
    //     if (ret == -1 && errno != EINPROGRESS) {
    //         ::close(fd_);
    //         fd_ = -1;
    //         continue;
    //     }

    //     ret = 0;
    //     break;
    // }

    // sk_assert((ret == 0 && fd_ != -1) || (ret != 0 && fd_ == -1));
    // freeaddrinfo(server);
    // return ret;
}

int socket::listen(const inet_address& addr, int backlog) {
    int ret = ::bind(fd_, addr.address(), addr.length());
    if (ret != 0) {
        ::close(fd_);
        fd_ = -1;
        return ret;
    }

    ret = ::listen(fd_, backlog);
    if (ret != 0) {
        ::close(fd_);
        fd_ = -1;
        return ret;
    }

    return 0;

    // struct addrinfo hints;
    // memset(&hints, 0x00, sizeof(hints));
    // hints.ai_family = AF_INET;
    // hints.ai_socktype = SOCK_STREAM;
    // hints.ai_flags = AI_PASSIVE;

    // struct addrinfo *server = nullptr;
    // std::string port_str = std::to_string(port);
    // int ret = getaddrinfo(addr.c_str(), port_str.c_str(), &hints, &server);
    // if (ret != 0) return ret;

    // for (struct addrinfo *p = server; p != nullptr; p = p->ai_next) {
    //     fd_ = make_socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    //     if (fd_ == -1) continue;

    //     ret = ::bind(fd_, p->ai_addr, p->ai_addrlen);
    //     if (ret != 0) {
    //         ::close(fd_);
    //         fd_ = -1;
    //         continue;
    //     }

    //     ret = ::listen(fd_, backlog);
    //     if (ret != 0) {
    //         ::close(fd_);
    //         fd_ = -1;
    //         continue;
    //     }

    //     ret = 0;
    //     break;
    // }

    // sk_assert((ret == 0 && fd_ != -1) || (ret != 0 && fd_ == -1));
    // freeaddrinfo(server);
    // return ret;
}

socket_ptr socket::accept(inet_address& addr) {
    int fd = -1;
    socklen_t addrlen = 0;

    while (1) {
        do {
            errno = 0;
            fd = ::accept(fd_, addr.address(), &addrlen);
        } while (fd == -1 && errno == EINTR);

        if (fd == -1)
            return nullptr;

        break;
    }

    int ret = set_nodelay(fd, true);
    if (ret != 0) {
        close(fd);
        return nullptr;
    }

    ret = set_nonblock(fd, true);
    if (ret != 0) {
        close(fd);
        return nullptr;
    }

    ret = set_cloexec(fd, true);
    if (ret != 0) {
        close(fd);
        return nullptr;
    }

    // TODO: may set_keepalive(...) here?

    socket_ptr s = create();
    if (unlikely(!s)) {
        errno = ENOMEM;
        return nullptr;
    }

    s->fd_ = fd;
    return s;
}

ssize_t socket::send(const void *buf, size_t len) {
    ssize_t nbytes = 0;
    do {
        nbytes = write(fd_, buf, len);
    } while (nbytes == -1 && errno == EINTR);

    if (nbytes < 0) {
        if (errno == EAGAIN || errno != EWOULDBLOCK) {
            // do nothing here, it's the caller's duty
            // to handle the may-block condition
        }
    }

    return nbytes;
}

ssize_t socket::recv(void *buf, size_t len) {
    ssize_t nbytes = 0;
    do {
        nbytes = read(fd_, buf, len);
    } while (nbytes == -1 && errno == EINTR);

    if (nbytes < 0) {
        if (errno == EAGAIN || errno != EWOULDBLOCK) {
            // do nothing here, it's the caller's duty
            // to handle the may-block condition
        }

        return nbytes;
    }

    // 0 => end of stream
    if (nbytes == 0)
        return nbytes;

    return nbytes;
}

int socket::get_error(int fd) {
    int value = 0;
    socklen_t len = static_cast<socklen_t>(sizeof(value));

    int ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &value, &len);
    if (ret == -1) return errno;

    return value;
}

int socket::set_reuseaddr(int fd) {
    int yes = 1;
    int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (ret == -1) return ret;

    return 0;
}

int socket::set_nodelay(int fd, bool on) {
    int yes = on ? 1 : 0;
    int ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    if (ret == -1) return ret;

    return 0;
}

int socket::set_cloexec(int fd, bool on) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) return flags;

    if (on)
        flags |= O_CLOEXEC;
    else
        flags &= ~O_CLOEXEC;

    int ret = fcntl(fd, F_SETFL, flags);
    if (ret == -1) return ret;

    return 0;
}

int socket::set_nonblock(int fd, bool on) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) return flags;

    if (on)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;

    int ret = fcntl(fd, F_SETFL, flags);
    if (ret == -1) return ret;

    return 0;
}

int socket::set_sndbuf(int fd, int size) {
    int ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
    if (ret == -1) return ret;

    return 0;
}

int socket::set_rcvbuf(int fd, int size) {
    int ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    if (ret == -1) return ret;

    return 0;
}

int socket::set_keepalive(int fd, int idle, int interval, int count) {
    int ret = 0;

    int yes = 1;
    ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
    if (ret == -1) return ret;

    // send first probe after "idle" second(s)
    ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    if (ret == -1) return ret;

    // send next probes after "interval" second(s)
    ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
    if (ret == -1) return ret;

    // consider the socket in error state after "count"
    // probes sent without getting a reply
    ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
    if (ret == -1) return ret;

    return 0;
}

int socket::make_socket(int domain, int type, int protocol) {
    int fd = ::socket(domain, type, protocol);
    if (fd == -1) return -1;

    int ret = 0;
    do {
        ret = set_cloexec(fd, true);
        check_break(ret == 0);

        ret = set_nonblock(fd, true);
        check_break(ret == 0);

        ret = set_reuseaddr(fd);
        check_break(ret == 0);
    } while (0);

    if (ret != 0) {
        ::close(fd);
        return -1;
    }

    return fd;
}

NS_END(sk)
