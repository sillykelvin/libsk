#ifndef FORMATTER_H
#define FORMATTER_H

#include "spdlog/formatter.h"
#include "spdlog/details/format.h"

namespace sk {

namespace detail {

class flag_formatter {
public:
    virtual ~flag_formatter() {}
    virtual void format(fmt::MemoryWriter& writer, const std::tm& time,
                        spdlog::level::level_enum level,
                        const char *file, int line,
                        const char *function, const char *msg) = 0;
};

} // namespace detail

class formatter {
public:
    explicit formatter(const std::string& pattern);

    void format(fmt::MemoryWriter& writer, const std::tm& time,
                spdlog::level::level_enum level,
                const char *file, int line,
                const char *function, const char *msg);

private:
    void handle_flag(char flag);
    void compile_pattern(const std::string& pattern);

private:
    std::vector<std::unique_ptr<detail::flag_formatter>> formatters_;

private:
    formatter(const formatter&) = delete;
    formatter& operator=(const formatter&) = delete;
};

} // namespace sk

#endif // FORMATTER_H
