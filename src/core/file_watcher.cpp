#include <unistd.h>
#include <string.h>
#include <log/log.h>
#include <sys/inotify.h>
#include <core/file_watcher.h>
#include <utility/assert_helper.h>

NS_BEGIN(sk)

file_watcher::file_watcher(int inotify_fd, uv_loop_t *loop)
    : inotify_fd_(inotify_fd),
      loop_(loop) {
    memset(&poll_, 0x00, sizeof(poll_));
    poll_.data = this;
}

file_watcher::~file_watcher() {
    for (const auto& it : wd2file_)
        inotify_rm_watch(inotify_fd_, it.first);

    wd2file_.clear();
    file2wd_.clear();

    if (inotify_fd_ != -1) {
        close(inotify_fd_);
        inotify_fd_ = -1;
    }
}

file_watcher *file_watcher::create(uv_loop_t *loop) {
    int ifd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (ifd == -1) return nullptr;

    return new file_watcher(ifd, loop);
}

void file_watcher::watch(const std::string& file) {
    auto it = file2wd_.find(file);
    if (it != file2wd_.end()) return;

    int wd = inotify_add_watch(inotify_fd_, file.c_str(), IN_ALL_EVENTS);
    sk_debug("watch file<%s>, wd<%d>.", file.c_str(), wd);

    file2wd_[file] = wd;
    wd2file_[wd] = file;
}

void file_watcher::unwatch(const std::string& file) {
    auto it = file2wd_.find(file);
    if (it == file2wd_.end()) return;

    int wd = it->second;
    inotify_rm_watch(inotify_fd_, wd);
    sk_debug("unwatch file<%s>, wd<%d>.", file.c_str(), wd);

    file2wd_.erase(it);
    wd2file_.erase(wd);
}

bool file_watcher::has_watch(const std::string& file) const {
    auto it = file2wd_.find(file);
    bool found = (it != file2wd_.end());
    if (found) sk_assert(wd2file_.find(it->second) != wd2file_.end());

    return found;
}

int file_watcher::start() {
    int ret = uv_poll_init(loop_, &poll_, inotify_fd_);
    if (ret != 0) {
        sk_error("uv_poll_init: %s", uv_err_name(ret));
        return ret;
    }

    ret = uv_poll_start(&poll_, UV_READABLE | UV_WRITABLE | UV_DISCONNECT, on_inotify_event);
    if (ret != 0) {
        sk_error("uv_poll_start: %s", uv_err_name(ret));
        return ret;
    }

    return 0;
}

int file_watcher::stop() {
    int ret = uv_poll_stop(&poll_);
    if (ret != 0) {
        sk_error("uv_poll_stop: %s", uv_err_name(ret));
        return ret;
    }

    return 0;
}

void file_watcher::on_inotify_read() {
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event = nullptr;
    ssize_t len = 0;

    while (true) {
        do {
            len = read(inotify_fd_, buf, sizeof(buf));
        } while (len == -1 && errno == EINTR);

        if (len == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                sk_error("inotify read error: %s.", strerror(errno));

            break;
        }

        if (len == 0) break;

        for (char *ptr = buf; ptr < buf + len;
             ptr += sizeof(struct inotify_event) + event->len) {
            event = reinterpret_cast<const struct inotify_event*>(ptr);

            if (event->mask == IN_IGNORED) continue;

            auto it = wd2file_.find(event->wd);
            if (it == wd2file_.end()) {
                sk_warn("file watch<%d> not found.", event->wd);
                continue;
            }

            sk_trace("OPEN(%d), CLOSE(%d), CHANGE(%d), DELETE(%d), file<%s>.",
                     event->mask & IN_OPEN ? 1 : 0, event->mask & IN_CLOSE ? 1 : 0,
                     event->mask & IN_MODIFY ? 1 : 0, event->mask & IN_DELETE_SELF ? 1 : 0,
                     it->second.c_str());

            if ((event->mask & IN_OPEN) && fn_on_file_open_)
                fn_on_file_open_(it->second);

            if ((event->mask & IN_MODIFY) && fn_on_file_change_)
                fn_on_file_change_(it->second);

            if ((event->mask & IN_CLOSE) && fn_on_file_close_)
                fn_on_file_close_(it->second);

            if ((event->mask & IN_DELETE_SELF) && fn_on_file_delete_)
                fn_on_file_delete_(it->second);
        }
    }
}

void file_watcher::on_inotify_event(uv_poll_t *handle, int status, int events) {
    file_watcher *w = cast_ptr(file_watcher, handle->data);
    sk_assert(&w->poll_ == handle);

    if (status != 0) {
        sk_error("on_inotify_event: %s", uv_err_name(status));
        return;
    }

    if (unlikely(events & UV_WRITABLE))
        sk_warn("writable events!");

    if (unlikely(events & UV_DISCONNECT))
        sk_warn("disconnect events!");

    if (events & UV_READABLE)
        return w->on_inotify_read();
}

NS_END(sk)
