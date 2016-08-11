#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include "log_config.h"
#include "spdlog/spdlog.h"

namespace sk {

class logger {
public:
    static const char *DEFAULT_LOGGER;

    static int init(const std::string& conf_file);

    static void log(const std::string& name, spdlog::level::level_enum level,
                    const char *file, int line, const char *function, const char *fmt, ...);

private:
    static logger _;

    static spdlog::sink_ptr make_sink(const log_config::file_device& dev) {
        // TODO: make a proper sink according to device here
        (void) dev;
        auto ptr = std::make_shared<spdlog::sinks::daily_file_sink_st>("server.log", "txt", 23, 59);
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
    log_config conf_;
};

#define sk_trace(fmt, ...)    logger::log(logger::DEFAULT_LOGGER, spdlog::level::trace,    __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define sk_debug(fmt, ...)    logger::log(logger::DEFAULT_LOGGER, spdlog::level::debug,    __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define sk_info(fmt, ...)     logger::log(logger::DEFAULT_LOGGER, spdlog::level::info,     __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define sk_warn(fmt, ...)     logger::log(logger::DEFAULT_LOGGER, spdlog::level::warn,     __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define sk_error(fmt, ...)    logger::log(logger::DEFAULT_LOGGER, spdlog::level::err,      __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define sk_critical(fmt, ...) logger::log(logger::DEFAULT_LOGGER, spdlog::level::critical, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

} // namespace sk

#endif // LOG_H
