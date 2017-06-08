#ifndef FILE_WATCHER_H
#define FILE_WATCHER_H

#include <unordered_map>
#include "net/handler.h"
#include "utility/types.h"

NS_BEGIN(sk)

class file_watcher {
public:
    using fn_on_file_event = std::function<void(const std::string& file)>;

    MAKE_NONCOPYABLE(file_watcher);

    ~file_watcher();

    static file_watcher *create(net::reactor *r);

    void watch(const std::string& file);
    void unwatch(const std::string& file);
    bool has_watch(const std::string& file) const;

    void start();
    void stop();

    void set_file_open_callback(const fn_on_file_event& fn)   { fn_on_file_open_   = fn; }
    void set_file_close_callback(const fn_on_file_event& fn)  { fn_on_file_close_  = fn; }
    void set_file_change_callback(const fn_on_file_event& fn) { fn_on_file_change_ = fn; }
    void set_file_delete_callback(const fn_on_file_event& fn) { fn_on_file_delete_ = fn; }

private:
    file_watcher(int inotify_fd, net::reactor *r);

    void on_inotify_read();

private:
    int inotify_fd_;
    std::string file_;
    net::handler_ptr handler_;

    fn_on_file_event fn_on_file_open_;
    fn_on_file_event fn_on_file_close_;
    fn_on_file_event fn_on_file_change_;
    fn_on_file_event fn_on_file_delete_;

    std::unordered_map<int, std::string> wd2file_;
    std::unordered_map<std::string, int> file2wd_;
};

NS_END(sk)

#endif // FILE_WATCHER_H
