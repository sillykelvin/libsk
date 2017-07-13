#ifndef SIGNAL_WATCHER_H
#define SIGNAL_WATCHER_H

#include <uv.h>
#include <unordered_set>
#include <core/callback.h>

NS_BEGIN(sk)

class signal_watcher {
public:
    MAKE_NONCOPYABLE(signal_watcher);

    ~signal_watcher();

    static signal_watcher *create(uv_loop_t *loop);

    int watch(int signal);
    int unwatch(int signal);

    bool has_watch(int signal) const {
        return watched_signals_.find(signal) != watched_signals_.end();
    }

    int start();
    int stop();

    void set_signal_callback(const fn_on_signal_event& fn) { fn_on_signal_ = fn; }

private:
    signal_watcher(int signal_fd, uv_loop_t *loop);

    void on_signal();
    static void on_signal_event(uv_poll_t *handle, int status, int events);

private:
    int signal_fd_;
    uv_poll_t poll_;
    uv_loop_t *loop_;

    fn_on_signal_event fn_on_signal_;

    std::unordered_set<int> watched_signals_;
};

NS_END(sk)

#endif // SIGNAL_WATCHER_H
