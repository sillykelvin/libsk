#ifndef SIGNAL_WATCHER_H
#define SIGNAL_WATCHER_H

#include <set>
#include <sys/signalfd.h>
#include "net/handler.h"
#include "utility/types.h"

NS_BEGIN(sk)

class signal_watcher {
public:
    using fn_on_signal_event = std::function<void(const signalfd_siginfo*)>;

    MAKE_NONCOPYABLE(signal_watcher);

    ~signal_watcher();

    static signal_watcher *create(net::reactor *r);

    int watch(int signal);
    int unwatch(int signal);

    bool has_watch(int signal) const {
        return watched_signals_.find(signal) != watched_signals_.end();
    }

    void start();
    void stop();

    void set_signal_callback(const fn_on_signal_event& fn) { fn_on_signal_ = fn; }

private:
    signal_watcher(int signal_fd, net::reactor *r);

    void on_signal();

private:
    int signal_fd_;
    net::handler_ptr handler_;

    fn_on_signal_event fn_on_signal_;

    std::set<int> watched_signals_;
};

NS_END(sk)

#endif // SIGNAL_WATCHER_H
