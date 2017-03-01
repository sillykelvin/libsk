#ifndef STRING_HELPER_H
#define STRING_HELPER_H

#include <vector>
#include <string>
#include "utility/types.h"

namespace sk {
namespace detail {

template<typename R, bool integral>
struct string_converter {
    R(*f_)(const char *, char **, int);

    string_converter(R(*func)(const char *, char **, int)) {
        f_ = func;
    }

    R operator()(const char *str, char **end) const {
        return f_(str, end, 10);
    }
};

template<typename R>
struct string_converter<R, false> {
    R(*f_)(const char *, char **);

    string_converter(R(*func)(const char *, char **)) {
        f_ = func;
    }

    R operator()(const char *str, char **end) const {
        return f_(str, end);
    }
};

template<typename T, typename R, bool integral>
static int convert(const char *str, T& val, const string_converter<R, integral>& f) {
    if (!str) return -EINVAL;

    errno = 0;
    char *end = NULL;
    val = f(str, &end);

    if (errno || *end != '\0' || end == str)
        return -EINVAL;

    return 0;
}

} // namespace detail

int split_string(const char *str, char delim, std::vector<std::string>& vec);
void replace_string(std::string& str, const std::string& src, const std::string& dst);
void fill_string(std::string& str, const char *fmt, ...);
void trim_string(std::string& str);
bool is_prefix(const std::string& str1, const std::string& str2);
std::string base64_decode(const std::string& str);
std::string md5_string(const std::string& str);

template<typename T>
struct string_traits {
    static std::string to_string(const T& t) {
        return t.to_string();
    }

    static int from_string(const char *str, T& t) {
        return t.from_string(str);
    }
};

#define define_trivial_string_traits(T, R, integral, conv_func) \
    template<>                                                  \
    struct string_traits<T> {                                   \
        static std::string to_string(const T& value) {          \
            return std::to_string(value);                       \
        }                                                       \
        static int from_string(const char *str, T& value) {     \
            detail::string_converter<R, integral> f(conv_func); \
            return detail::convert(str, value, f);              \
        }                                                       \
    }

define_trivial_string_traits(s32, long, true, strtol);
define_trivial_string_traits(u32, unsigned long, true, strtoul);
define_trivial_string_traits(s64, long long, true, strtoll);
define_trivial_string_traits(u64, unsigned long long, true, strtoull);
define_trivial_string_traits(float, float, false, strtof);
define_trivial_string_traits(double, double, false, strtod);

#undef define_trivial_string_traits

} // namespace sk

#endif // STRING_HELPER_H
