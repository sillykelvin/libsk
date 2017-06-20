#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include "log_config.h"
#include "spdlog/spdlog.h"
#include "formatter.h"
#include "file_sink.h"

NS_BEGIN(sk)

class logger {
public:
    static const char *DEFAULT_LOGGER;

    static int init(const std::string& conf_file);

    static int reload();

    static bool level_enabled(const std::string& name, spdlog::level::level_enum level);

    static void log(const std::string& name, spdlog::level::level_enum level,
                    const char *file, int line, const char *function, const char *fmt, ...);

private:
    static logger _;

    static spdlog::sink_ptr make_sink(const log_config::file_device& dev) {
        auto ptr = std::make_shared<file_sink_st>(dev.path, dev.max_size, dev.max_rotation, true);
        return ptr;
    }

    static spdlog::sink_ptr make_sink(const log_config::net_device& dev) {
        // TODO: make a proper sink according to device here
        (void) dev;
        auto ptr = std::make_shared<spdlog::sinks::stdout_sink_st>();
        return ptr;
    }

    static std::shared_ptr<spdlog::logger> make_logger(const log_config::category& cat);

    static spdlog::level::level_enum string2level(const std::string& level);

private:
    std::string conf_file_;
    log_config conf_;
    std::map<std::string,
             std::pair<std::unique_ptr<formatter>,
                       std::shared_ptr<spdlog::logger>>> name2logger_;
};

#define sk_trace_enabled() sk::logger::level_enabled(sk::logger::DEFAULT_LOGGER, spdlog::level::trace)
#define sk_debug_enabled() sk::logger::level_enabled(sk::logger::DEFAULT_LOGGER, spdlog::level::debug)
#define sk_info_enabled()  sk::logger::level_enabled(sk::logger::DEFAULT_LOGGER, spdlog::level::info)
#define sk_warn_enabled()  sk::logger::level_enabled(sk::logger::DEFAULT_LOGGER, spdlog::level::warn)
#define sk_error_enabled() sk::logger::level_enabled(sk::logger::DEFAULT_LOGGER, spdlog::level::err)
#define sk_fatal_enabled() sk::logger::level_enabled(sk::logger::DEFAULT_LOGGER, spdlog::level::critical)

#define sk_trace(fmt, ...) sk::logger::log(sk::logger::DEFAULT_LOGGER, spdlog::level::trace,    __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define sk_debug(fmt, ...) sk::logger::log(sk::logger::DEFAULT_LOGGER, spdlog::level::debug,    __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define sk_info(fmt, ...)  sk::logger::log(sk::logger::DEFAULT_LOGGER, spdlog::level::info,     __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define sk_warn(fmt, ...)  sk::logger::log(sk::logger::DEFAULT_LOGGER, spdlog::level::warn,     __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define sk_error(fmt, ...) sk::logger::log(sk::logger::DEFAULT_LOGGER, spdlog::level::err,      __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define sk_fatal(fmt, ...) sk::logger::log(sk::logger::DEFAULT_LOGGER, spdlog::level::critical, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

NS_END(sk)

#endif // LOG_H
