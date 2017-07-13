#ifndef FILE_WATCHER_H
#define FILE_WATCHER_H

#include <uv.h>
#include <unordered_map>
#include <core/callback.h>

NS_BEGIN(sk)

class file_watcher {
public:
    MAKE_NONCOPYABLE(file_watcher);

    ~file_watcher();

    static file_watcher *create(uv_loop_t *loop);

    void watch(const std::string& file);
    void unwatch(const std::string& file);
    bool has_watch(const std::string& file) const;

    int start();
    int stop();

    void set_file_open_callback(const fn_on_file_event& fn)   { fn_on_file_open_   = fn; }
    void set_file_close_callback(const fn_on_file_event& fn)  { fn_on_file_close_  = fn; }
    void set_file_change_callback(const fn_on_file_event& fn) { fn_on_file_change_ = fn; }
    void set_file_delete_callback(const fn_on_file_event& fn) { fn_on_file_delete_ = fn; }

private:
    file_watcher(int inotify_fd, uv_loop_t *loop);

    void on_inotify_read();
    static void on_inotify_event(uv_poll_t *handle, int status, int events);

private:
    int inotify_fd_;
    uv_poll_t poll_;
    uv_loop_t *loop_;

    fn_on_file_event fn_on_file_open_;
    fn_on_file_event fn_on_file_close_;
    fn_on_file_event fn_on_file_change_;
    fn_on_file_event fn_on_file_delete_;

    std::unordered_map<int, std::string> wd2file_;
    std::unordered_map<std::string, int> file2wd_;
};

NS_END(sk)

#endif // FILE_WATCHER_H
