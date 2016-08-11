#include "log.h"

namespace sk {

int logger::init(const std::string& conf_file) {
    // TODO: load conf from conf file here

    bool found = false;
    for (auto it : _.conf_.categories) {
        if (it->name == DEFAULT_LOGGER) {
            found = true;
            break;
        }
    }

    if (!found) {
        log_config::category cat;
        cat.name = DEFAULT_LOGGER;
        cat.pattern = "[%Y%m%d %H:%M:%S.%f][%l] %v";
        cat.level = "DEBUG";
        _.conf_.categories.push_back(cat);
    }

    int count = 0;

    try {
        for (auto it : _.conf_.categories) {
            auto ptr = make_logger(*it);
            if (ptr) {
                count += 1;
                spdlog::register_logger(ptr);
            }
        }
    } catch (const spdlog::spdlog_ex& e) {
        fprintf(stderr, "exception: %s\n", e.what());
        return -EINVAL;
    }

    if (count <= 0) {
        fprintf(stderr, "at least one logger is required.\n");
        return -EINVAL;
    }

    auto def = spdlog::get(DEFAULT_LOGGER);
    if (!def) {
        fprintf(stderr, "a logger with name \"%s\" is required.\n", DEFAULT_LOGGER);
        return -EINVAL;
    }

    return 0;
}

void logger::log(const std::string& name, spdlog::level::level_enum level,
                 const char *fmt, const char *file, int line, const char *function, ...) {
    static char buffer[40960]; // 40KB

    auto l = spdlog::get(name);
    if (!l) {
        log(DEFAULT_LOGGER, spdlog::level::warn, "logger %s not found.", file, line, function, name.c_str());
        return;
    }

    // TODO: we may process the pattern here, because the
    // pattern in spdlog does not have file/line/function definitions

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    buffer[sizeof(buffer) - 1] = 0;

    try {
        l->log(level, "[({}:{}) ({})] {}", file, line, function, buffer);
    } catch (const spdlog::spdlog_ex& e) {
        // must NOT call log(...) here
    }
}

std::shared_ptr<spdlog::logger> logger::make_logger(const log_config::category& cat) {
    std::vector<spdlog::sink_ptr> sinks;

    for (auto it : cat.fdev_list) {
        auto ptr = make_sink(*it);
        if (ptr) sinks.push_back(ptr);
    }

    for (auto it : cat.ndev_list) {
        auto ptr = make_sink(*it);
        if (ptr) sinks.push_back(ptr);
    }

    auto ptr = std::make_shared<spdlog::logger>(cat.name, sinks.begin(), sinks.end());
    if (!ptr) {
        fprintf(stderr, "cannot init logger, name: %s\n", cat.name.c_str());
        return NULL;
    }

    ptr->set_level(string2level(cat.level));
    ptr->set_pattern(cat.pattern);

    return ptr;
}

spdlog::level::level_enum logger::string2level(const std::string& level) {
    if (level == "TRACE")
        return spdlog::level::trace;

    if (level == "DEBUG")
        return spdlog::level::debug;

    if (level == "INFO")
        return spdlog::level::info;

    if (level == "WARN")
        return spdlog::level::warn;

    if (level == "ERROR")
        return spdlog::level::err;

    if (level == "CRITICAL")
        return spdlog::level::critical;

    fprintf(stderr, "invalid level: %s\n", level.c_str());
    return spdlog::level::trace;
}

} // namespace sk
