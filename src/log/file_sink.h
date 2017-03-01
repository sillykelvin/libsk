#ifndef FILE_SINK_H
#define FILE_SINK_H

#include <ctime>
#include "spdlog/sinks/base_sink.h"
#include "spdlog/details/file_helper.h"
#include "utility/types.h"

namespace sk {

template<class Mutex>
class file_sink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit file_sink(const std::string& path_pattern, u32 max_size,
                       u32 max_rotation, bool force_flush = false) :
        path_pattern_(path_pattern),
        max_size_(max_size),
        max_rotation_(max_rotation),
        file_helper_(force_flush),
        current_path_(base_path(path_pattern)),
        current_size_(0)
    {
        ensure_directory(current_path_);
        file_helper_.open(current_path_);
        current_size_ = file_helper_.size();
    }

    void flush() override {
        file_helper_.flush();
    }

protected:
    void _sink_it(const spdlog::details::log_msg& msg) override {
        std::string bpath = base_path(path_pattern_);
        if (bpath != current_path_) {
            current_path_ = bpath;
            file_helper_.close();
            ensure_directory(current_path_);
            file_helper_.open(current_path_, true);
            current_size_ = 0;
        }

        current_size_ += msg.formatted.size();
        if (current_size_ > max_size_) {
            file_helper_.close();
            rotate();
            file_helper_.reopen(true);
            current_size_ = msg.formatted.size();
        }

        file_helper_.write(msg);
    }

private:
    void rotate() {
        for (auto i = max_rotation_; i > 0; --i) {
            std::string src = real_path(path_pattern_, i - 1);
            std::string dst = real_path(path_pattern_, i);

            if (spdlog::details::file_helper::file_exists(dst)) {
                if (spdlog::details::os::remove(dst) != 0)
                    throw spdlog::spdlog_ex("sk::file_sink: failed removing " + dst);
            }

            if (spdlog::details::file_helper::file_exists(src)) {
                if (spdlog::details::os::rename(src, dst) != 0)
                    throw spdlog::spdlog_ex("sk::file_sink: failed renaming " + src + " to " + dst);
            }
        }
    }

    static void ensure_directory(const std::string& path) {
        auto pos = path.rfind('/');
        if (pos == std::string::npos) // current dir, must exist
            return;

        std::string dir = path.substr(0, pos);
        if (dir.empty())
            return;

        std::string::size_type last = 0;
        while (true) {
            pos = dir.find('/', last);
            if (pos == 0) {
                last = pos + 1;
                continue;
            }

            if (pos == std::string::npos)
                break;

            dir.at(pos) = '\0';
            int ret = mkdir(dir.c_str(), 0755);
            if (ret != 0 && errno != EEXIST)
                throw spdlog::spdlog_ex("Failed creating directory " + dir + ", error: " + strerror(errno));

            dir.at(pos) = '/';
            last = pos + 1;
        }

        int ret = mkdir(dir.c_str(), 0755);
        if (ret != 0 && errno != EEXIST)
            throw spdlog::spdlog_ex("Failed creating directory " + dir + ", error: " + strerror(errno));
    }

    static std::string base_path(const std::string& path_pattern) {
        std::time_t now = std::time(NULL);
        std::tm *tm = std::localtime(&now);

        char buf[256] = {0};
        strftime(buf, sizeof(buf), path_pattern.c_str(), tm);

        return std::string(buf);
    }

    static std::string real_path(const std::string& path_pattern, size_t index) {
        std::string bpath = base_path(path_pattern);

        if (index <= 0)
            return bpath;

        char buf[256] = {0};
        snprintf(buf, sizeof(buf), "%s.%lu", bpath.c_str(), index);

        return std::string(buf);
    }

private:
    std::string path_pattern_;
    u32 max_size_;
    u32 max_rotation_;
    spdlog::details::file_helper file_helper_;

    std::string current_path_;
    size_t current_size_;
};

typedef file_sink<std::mutex> file_sink_mt;
typedef file_sink<spdlog::details::null_mutex> file_sink_st;

} // namespace sk

#endif // FILE_SINK_H
