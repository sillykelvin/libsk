#include <string.h>
#include <log/log.h>
#include <core/signal_watcher.h>
#include <utility/assert_helper.h>

NS_BEGIN(sk)

signal_watcher::signal_watcher(int signal_fd, uv_loop_t *loop)
    : signal_fd_(signal_fd),
      loop_(loop) {
    memset(&poll_, 0x00, sizeof(poll_));
    poll_.data = this;
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

signal_watcher *signal_watcher::create(uv_loop_t *loop) {
    sigset_t mask;
    sigemptyset(&mask);

    int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd == -1) return nullptr;

    return new signal_watcher(sfd, loop);
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

int signal_watcher::start() {
    int ret = uv_poll_init(loop_, &poll_, signal_fd_);
    if (ret != 0) {
        sk_error("uv_poll_init: %s", uv_err_name(ret));
        return ret;
    }

    ret = uv_poll_start(&poll_, UV_READABLE | UV_WRITABLE | UV_DISCONNECT, on_signal_event);
    if (ret != 0) {
        sk_error("uv_poll_start: %s", uv_err_name(ret));
        return ret;
    }

    return 0;
}

int signal_watcher::stop() {
    int ret = uv_poll_stop(&poll_);
    if (ret != 0) {
        sk_error("uv_poll_stop: %s", uv_err_name(ret));
        return ret;
    }

    return 0;
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

void signal_watcher::on_signal_event(uv_poll_t *handle, int status, int events) {
    signal_watcher *w = cast_ptr(signal_watcher, handle->data);
    sk_assert(&w->poll_ == handle);

    if (status != 0) {
        sk_error("on_signal_event: %s", uv_err_name(status));
        return;
    }

    if (unlikely(events & UV_WRITABLE))
        sk_warn("writable events!");

    if (unlikely(events & UV_DISCONNECT))
        sk_warn("disconnect events!");

    if (events & UV_READABLE)
        return w->on_signal();
}

NS_END(sk)
