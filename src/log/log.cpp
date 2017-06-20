#include "log.h"
#include "utility/assert_helper.h"

namespace sk {

const char *logger::DEFAULT_LOGGER = "default";
logger logger::_;

int logger::init(const std::string& conf_file) {
    _.conf_file_ = conf_file;
    _.conf_.categories.clear();
    _.name2logger_.clear();

    int ret = _.conf_.load_from_xml_file(conf_file.c_str());
    if (ret != 0) return ret;

    bool found = false;
    for (const auto& it : _.conf_.categories) {
        if (it.name == DEFAULT_LOGGER) {
            found = true;
            break;
        }
    }

    if (!found) {
        log_config::category cat;
        cat.name = DEFAULT_LOGGER;
        cat.pattern = "[%T][%L][(%f:%l) (%F)] %m";
        cat.level = "DEBUG";

        log_config::file_device dev;
        dev.path = "./%Y%m%d/%H/server.log";
        dev.max_size = 50 * 1024 * 1024;
        dev.max_rotation = 100;

        cat.fdev_list.push_back(dev);
        _.conf_.categories.push_back(cat);
    }

    int count = 0;

    try {
        for (const auto& it : _.conf_.categories) {
            auto ptr = make_logger(it);
            if (ptr) {
                count += 1;
                std::unique_ptr<formatter> f(new formatter(it.pattern));
                _.name2logger_[it.name] = std::move(std::make_pair<std::unique_ptr<formatter>,
                                                                   std::shared_ptr<spdlog::logger>>(std::move(f),
                                                                                                    std::move(ptr)));
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

    auto it = _.name2logger_.find(DEFAULT_LOGGER);
    if (it == _.name2logger_.end()) {
        fprintf(stderr, "a logger with name \"%s\" is required.\n", DEFAULT_LOGGER);
        return -EINVAL;
    }

    return 0;
}

int logger::reload() {
    std::string conf_file = _.conf_file_;
    log_config conf = _.conf_;
    std::map<std::string,
             std::pair<std::unique_ptr<formatter>,
                       std::shared_ptr<spdlog::logger>>> name2logger;
    for (auto& it : _.name2logger_)
        name2logger[it.first] = std::move(it.second);

    int ret = init(conf_file);
    if (ret == 0)
        return ret;

    _.conf_file_ = conf_file;
    _.conf_ = conf;
    for (auto& it : name2logger)
        _.name2logger_[it.first] = std::move(it.second);

    return ret;
}

bool logger::level_enabled(const std::string& name, spdlog::level::level_enum level) {
    auto it = _.name2logger_.find(name);
    return it != _.name2logger_.end() && it->second.second->should_log(level);
}

void logger::log(const std::string& name, spdlog::level::level_enum level,
                 const char *file, int line, const char *function, const char *fmt, ...) {
    static char buffer[40960]; // 40KB
    static fmt::MemoryWriter writer;

    auto it = _.name2logger_.find(name);
    if (it == _.name2logger_.end()) {
        it = _.name2logger_.find(DEFAULT_LOGGER);
        if (it != _.name2logger_.end())
            log(DEFAULT_LOGGER, spdlog::level::warn, file, line, function, "logger %s not found.", name.c_str());

        return;
    }

    if (!it->second.second->should_log(level))
        return;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    buffer[sizeof(buffer) - 1] = 0;

    writer.clear();
    it->second.first->format(writer, spdlog::details::os::localtime(), level, file, line, function, buffer);

    try {
        it->second.second->log(level, "{}", fmt::StringRef(writer.data(), writer.size()));
    } catch (const spdlog::spdlog_ex& e) {
        // must NOT call log(...) here
    }
}

std::shared_ptr<spdlog::logger> logger::make_logger(const log_config::category& cat) {
    std::vector<spdlog::sink_ptr> sinks;

    for (const auto& it : cat.fdev_list) {
        auto ptr = make_sink(it);
        if (ptr) sinks.push_back(ptr);
    }

    for (const auto& it : cat.ndev_list) {
        auto ptr = make_sink(it);
        if (ptr) sinks.push_back(ptr);
    }

    if (sinks.empty()) {
        fprintf(stderr, "no available sink.\n");
        return NULL;
    }

    auto ptr = std::make_shared<spdlog::logger>(cat.name, sinks.begin(), sinks.end());
    if (!ptr) {
        fprintf(stderr, "cannot init logger, name: %s\n", cat.name.c_str());
        return NULL;
    }

    ptr->set_level(string2level(cat.level));
    ptr->set_pattern("%v"); // we will do formatting ourselves

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
