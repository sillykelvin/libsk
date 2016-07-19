#include <errno.h>
#include "string_helper.h"

namespace sk {

template<typename R, bool integral>
struct functor {
    R(*f_)(const char *, char **, int);

    functor(R(*func)(const char *, char **, int)) {
        f_ = func;
    }

    R operator()(const char *str, char **end) const {
        return f_(str, end, 10);
    }
};

template<typename R>
struct functor<R, false> {
    R(*f_)(const char *, char **);

    functor(R(*func)(const char *, char **)) {
        f_ = func;
    }

    R operator()(const char *str, char **end) const {
        return f_(str, end);
    }
};

template<typename T, typename R, bool integral>
static int convert(const char *str, T& val, const functor<R, integral>& f) {
    if (!str) return -EINVAL;

    errno = 0;
    char *end = NULL;
    val = f(str, &end);

    if (errno || *end != '\0' || end == str)
        return -EINVAL;

    return 0;
}

int string2s32(const char *str, s32& val) {
    functor<long, true> f(strtol);
    return convert(str, val, f);
}

int string2u32(const char *str, u32& val) {
    functor<unsigned long, true> f(strtoul);
    return convert(str, val, f);
}

int string2s64(const char *str, s64& val) {
    functor<long long, true> f(strtoll);
    return convert(str, val, f);
}

int string2u64(const char *str, u64& val) {
    functor<unsigned long long, true> f(strtoull);
    return convert(str, val, f);
}

int string2float(const char *str, float &val) {
    functor<float, false> f(strtof);
    return convert(str, val, f);
}

int string2double(const char *str, double &val) {
    functor<double, false> f(strtod);
    return convert(str, val, f);
}

} // namespace sk
