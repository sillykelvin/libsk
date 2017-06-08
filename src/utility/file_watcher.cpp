#include <unistd.h>
#include <sys/inotify.h>
#include "log/log.h"
#include "file_watcher.h"
#include "utility/assert_helper.h"

NS_BEGIN(sk)

file_watcher::file_watcher(int inotify_fd, net::reactor *r)
    : inotify_fd_(inotify_fd),
      handler_(new net::handler(r, inotify_fd)) {
    handler_->set_read_callback(std::bind(&file_watcher::on_inotify_read, this));
}

file_watcher::~file_watcher() {
    for (const auto& it : fd2file_)
        inotify_rm_watch(inotify_fd_, it.first);

    if (inotify_fd_ != -1) {
        close(inotify_fd_);
        inotify_fd_ = -1;
    }
}

file_watcher *file_watcher::create(sk::net::reactor *r) {
    int ifd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (ifd == -1) return nullptr;

    return new file_watcher(ifd, r);
}

void file_watcher::watch(const std::string& file) {
    auto it = file2fd_.find(file);
    if (it != file2fd_.end()) return;

    int fd = inotify_add_watch(inotify_fd_, file.c_str(), IN_ALL_EVENTS);
    sk_debug("watch file<%s>, fd<%d>.", file.c_str(), fd);

    file2fd_[file] = fd;
    fd2file_[fd] = file;
}

void file_watcher::unwatch(const std::string& file) {
    auto it = file2fd_.find(file);
    if (it == file2fd_.end()) return;

    int fd = it->second;
    inotify_rm_watch(inotify_fd_, fd);
    sk_debug("unwatch file<%s>, fd<%d>.", file.c_str(), fd);

    file2fd_.erase(it);
    fd2file_.erase(fd);
}

bool file_watcher::has_watch(const std::string& file) const {
    auto it = file2fd_.find(file);
    bool found = (it != file2fd_.end());
    if (found) sk_assert(fd2file_.find(it->second) != fd2file_.end());

    return found;
}

void file_watcher::start() {
    handler_->enable_reading();
}

void file_watcher::stop() {
    handler_->disable_reading();
}

void file_watcher::on_inotify_read() {
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event = nullptr;

    while (true) {
        ssize_t len = read(inotify_fd_, buf, sizeof(buf));
        if (len == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                sk_error("inotify read error: %s.", strerror(errno));

            break;
        }

        if (len == 0) break;

        for (char *ptr = buf; ptr < buf + len;
             ptr += sizeof(struct inotify_event) + event->len) {
            event = reinterpret_cast<const struct inotify_event*>(ptr);

            if (event->mask == IN_IGNORED) {
                sk_debug("IN_IGNORED, %s", event->len > 0 ? event->name : "unknown");
                continue;
            }

            auto it = fd2file_.find(event->wd);
            if (it == fd2file_.end()) {
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

NS_END(sk)
