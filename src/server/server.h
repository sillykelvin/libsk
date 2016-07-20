#ifndef SERVER_H
#define SERVER_H

#include <memory>
#include "spdlog/spdlog.h"
#include "server/option_parser.h"

namespace sk {

struct server_context {
    u64 id;                // id of this server, it is also the bus id
    std::string str_id;    // string id of this server, like "x.x.x.x"
    std::string pid_file;  // pid file location
    std::string log_conf;  // log config location
    std::string proc_conf; // process config location
    bool resume_mode;      // if the process is running under resume mode

    server_context() : id(0), resume_mode(false) {}
};

template<typename Config>
class server {
public:
    server() {}
    virtual ~server() {}

    server(const server&) = delete;
    server& operator=(const server&) = delete;

    int init(int argc, const char **argv);
    int fini();
    int stop();
    int run();
    int reload();
    void usage();

    const Config& config() const { return conf_; }
    const server_context& ctx() const { return ctx_; }

    template <typename... Args>
    void log(spdlog::level::level_enum lvl, const char* fmt, const Args&... args) {
        logger_->log(lvl, fmt, args...);
    }

    template <typename... Args>
    void log(spdlog::level::level_enum lvl, const char* msg) {
        logger_->log(lvl, msg);
    }

    template <typename... Args>
    void trace(const char* fmt, const Args&... args) {
        logger_->trace(fmt, args...);
    }

    template <typename... Args>
    void debug(const char* fmt, const Args&... args) {
        logger_->debug(fmt, args...);
    }

    template <typename... Args>
    void info(const char* fmt, const Args&... args) {
        logger_->info(fmt, args...);
    }

    template <typename... Args>
    void warn(const char* fmt, const Args&... args) {
        logger_->warn(fmt, args...);
    }

    template <typename... Args>
    void error(const char* fmt, const Args&... args) {
        logger_->error(fmt, args...);
    }

    template <typename... Args>
    void critical(const char* fmt, const Args&... args) {
        logger_->critical(fmt, args...);
    }

protected:
    virtual int on_init() = 0;
    virtual int on_fini() = 0;
    virtual int on_stop() = 0;
    virtual int on_run() = 0;
    virtual int on_reload() = 0;

private:
    int init_parser();
    int init_ctx(const char *program);
    int init_logger();
    int init_conf() {
        return conf_->load_from_xml_file(ctx_.proc_conf.c_str());
    }

    bool lock_pid();
    int write_pid();

private:
    Config conf_;
    server_context ctx_;
    option_parser parser_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace sk

#endif // SERVER_H
