#include <signal.h>
#include <unistd.h>
#include "log/log.h"
#include "signal_watcher.h"
#include "utility/assert_helper.h"

NS_BEGIN(sk)

signal_watcher::signal_watcher(int signal_fd, net::reactor *r)
    : signal_fd_(signal_fd),
      handler_(new net::handler(r, signal_fd)) {
    handler_->set_read_callback(std::bind(&signal_watcher::on_signal, this));
}

signal_watcher::~signal_watcher() {
    sigset_t mask;
    sigemptyset(&mask);
    for (const auto& sig : watched_signals_)
        sigaddset(&mask, sig);

    if (sigprocmask(SIG_UNBLOCK, &mask, nullptr) == -1)
        sk_fatal("cannot unblock signals, error: %s", strerror(errno));

    watched_signals_.clear();

    if (signal_fd_ != -1) {
        close(signal_fd_);
        signal_fd_ = -1;
    }
}

signal_watcher *signal_watcher::create(net::reactor *r) {
    sigset_t mask;
    sigemptyset(&mask);

    int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd == -1) return nullptr;

    return new signal_watcher(sfd, r);
}

int signal_watcher::watch(int signal) {
    auto it = watched_signals_.find(signal);
    if (it != watched_signals_.end()) return 0;

    sigset_t mask;
    sigemptyset(&mask);

    sigaddset(&mask, signal);
    if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1) {
        sk_error("cannot block signal<%d>, error<%s>.", signal, strerror(errno));
        return -1;
    }

    for (const auto& sig : watched_signals_)
        sigaddset(&mask, sig);

    int sfd = signalfd(signal_fd_, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd == -1) {
        sk_error("cannot watch signal<%d>, error<%s>.", signal, strerror(errno));

        // don't forget to unblock the signal here
        sigemptyset(&mask);
        sigaddset(&mask, signal);
        sigprocmask(SIG_UNBLOCK, &mask, nullptr);

        return -1;
    }

    sk_debug("signal<%d> watched.", signal);
    watched_signals_.insert(signal);
    return 0;
}

int signal_watcher::unwatch(int signal) {
    auto it = watched_signals_.find(signal);
    if (it == watched_signals_.end()) return 0;

    sigset_t mask;
    sigemptyset(&mask);

    for (const auto& sig : watched_signals_) {
        if (sig != signal) sigaddset(&mask, sig);
    }

    int sfd = signalfd(signal_fd_, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd == -1) {
        sk_error("cannot unwatch signal<%d>, error<%s>.", signal, strerror(errno));
        return -1;
    }

    sigemptyset(&mask);
    sigaddset(&mask, signal);
    sigprocmask(SIG_UNBLOCK, &mask, nullptr);

    sk_debug("signal<%d> unwatched.", signal);
    watched_signals_.erase(it);
    return 0;
}

void signal_watcher::start() {
    handler_->enable_reading();
}

void signal_watcher::stop() {
    handler_->disable_reading();
}

void signal_watcher::on_signal() {
    char buf[4096] __attribute__((aligned(__alignof__(struct signalfd_siginfo))));
    const struct signalfd_siginfo *siginfo = nullptr;
    ssize_t len = 0;

    while (true) {
        do {
            len = read(signal_fd_, buf, sizeof(buf));
        } while(len == -1 && errno == EINTR);

        if (len == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                sk_error("signal read error: %s", strerror(errno));

            break;
        }

        if (len == 0) break;

        for (char *ptr = buf; ptr < buf + len;
             ptr += sizeof(struct signalfd_siginfo)) {
            siginfo = reinterpret_cast<const struct signalfd_siginfo*>(ptr);
            if (fn_on_signal_) fn_on_signal_(siginfo);
        }
    }
}

NS_END(sk)
