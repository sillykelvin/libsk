#include "formatter.h"
#include "spdlog/details/os.h"

using namespace sk::detail;

static const char* level_names[] = { "TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL", "OFF  " };

// time format (%T), e.g.: 20160918 14:45:20.777923
class T_formatter : public flag_formatter {
    void format(fmt::MemoryWriter& writer, const std::tm& time,
                spdlog::level::level_enum level,
                const char *file, int line,
                const char *function, const char *msg) override {
        auto duration = spdlog::details::os::now().time_since_epoch();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() % 1000000;

        writer << time.tm_year + 1900
               << fmt::pad(time.tm_mon +1, 2, '0')
               << fmt::pad(time.tm_mday,   2, '0')
               << ' '
               << fmt::pad(time.tm_hour,   2, '0')
               << ':'
               << fmt::pad(time.tm_min,    2, '0')
               << ':'
               << fmt::pad(time.tm_sec,    2, '0')
               << '.'
               << fmt::pad(static_cast<int>(micros), 6, '0');
    }
};

// message level, e.g.: DEBUG
class L_formatter : public flag_formatter {

    void format(fmt::MemoryWriter& writer, const std::tm& time,
                spdlog::level::level_enum level,
                const char *file, int line,
                const char *function, const char *msg) override {
        writer << level_names[level];
    }
};

// source file
class f_formatter : public flag_formatter {
    void format(fmt::MemoryWriter& writer, const std::tm& time,
                spdlog::level::level_enum level,
                const char *file, int line,
                const char *function, const char *msg) override {
        if (!file) file = "Unknown File";

        writer << file;
    }
};

// file line
class l_formatter : public flag_formatter {
    void format(fmt::MemoryWriter& writer, const std::tm& time,
                spdlog::level::level_enum level,
                const char *file, int line,
                const char *function, const char *msg) override {
        writer << line;
    }
};

// function, e.g.: main
class F_formatter : public flag_formatter {
    void format(fmt::MemoryWriter& writer, const std::tm& time,
                spdlog::level::level_enum level,
                const char *file, int line,
                const char *function, const char *msg) override {
        if (!function) function = "Unknown Function";

        writer << function;
    }
};

// message
class m_formatter : public flag_formatter {
    void format(fmt::MemoryWriter& writer, const std::tm& time,
                spdlog::level::level_enum level,
                const char *file, int line,
                const char *function, const char *msg) override {
        writer << msg;
    }
};

class string_formatter : public flag_formatter {
public:
    string_formatter() {}

    void add_char(char c) { str_ += c; }

    void format(fmt::MemoryWriter& writer, const std::tm& time,
                spdlog::level::level_enum level,
                const char *file, int line,
                const char *function, const char *msg) override {
        writer << str_;
    }

private:
    std::string str_;
};

namespace sk {

formatter::formatter(const std::string& pattern) {
    compile_pattern(pattern);
}

void formatter::format(fmt::MemoryWriter& writer, const std::tm& time,
                       spdlog::level::level_enum level,
                       const char *file, int line,
                       const char *function, const char *msg) {
    try {
        for (auto& f : formatters_)
            f->format(writer, time, level, file, line, function, msg);
    } catch (const fmt::FormatError& e) {
        throw spdlog::spdlog_ex(fmt::format("formatting error while processing format string: {}", e.what()));
    }
}

void formatter::handle_flag(char flag) {
    switch (flag) {
    case 'T':
        formatters_.push_back(std::unique_ptr<flag_formatter>(new T_formatter()));
        break;
    case 'L':
        formatters_.push_back(std::unique_ptr<flag_formatter>(new L_formatter()));
        break;
    case 'f':
        formatters_.push_back(std::unique_ptr<flag_formatter>(new f_formatter()));
        break;
    case 'l':
        formatters_.push_back(std::unique_ptr<flag_formatter>(new l_formatter()));
        break;
    case 'F':
        formatters_.push_back(std::unique_ptr<flag_formatter>(new F_formatter()));
        break;
    case 'm':
        formatters_.push_back(std::unique_ptr<flag_formatter>(new m_formatter()));
        break;
    default: {
        auto f = new string_formatter();
        f->add_char('%');
        f->add_char(flag);
        formatters_.push_back(std::unique_ptr<flag_formatter>(f));
        break;
    }
    }
}

void formatter::compile_pattern(const std::string& pattern) {
    auto end = pattern.end();
    std::unique_ptr<string_formatter> user_string;
    for (auto it = pattern.begin(); it != end; ++it) {
        if (*it == '%') {
            if (user_string)
                formatters_.push_back(std::move(user_string));

            if (++it != end)
                handle_flag(*it);
            else
                break;
        } else {
            if (!user_string)
                user_string = std::unique_ptr<string_formatter>(new string_formatter());
            user_string->add_char(*it);
        }
    }

    if (user_string)
        formatters_.push_back(std::move(user_string));
}

} // namespace sk
