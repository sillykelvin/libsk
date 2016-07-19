#include <errno.h>
#include "string_helper.h"

namespace sk {

template<typename T, typename R, R(*func)(const char *, char **, int)>
static int convert(const char *str, T& val) {
    if (!str) return -EINVAL;

    errno = 0;
    char *end = NULL;
    val = func(str, &end, 10);

    if (errno || *end != '\0' || end == str)
        return -EINVAL;

    return 0;
}

template<typename T, typename R, R(*func)(const char *, char **)>
static int convert(const char *str, T& val) {
    if (!str) return -EINVAL;

    errno = 0;
    char *end = NULL;
    val = func(str, &end);

    if (errno || *end != '\0' || end == str)
        return -EINVAL;

    return 0;
}

int string2s32(const char *str, s32& val) {
    return convert<s32, long, strtol>(str, val);
}

int string2u32(const char *str, u32& val) {
    return convert<u32, unsigned long, strtoul>(str, val);
}

int string2s64(const char *str, s64& val) {
    return convert<s64, long long, strtoll>(str, val);
}

int string2u64(const char *str, u64& val) {
    return convert<u64, unsigned long long, strtoull>(str, val);
}

int string2float(const char *str, float &val) {
    return convert<float, float, strtof>(str, val);
}

int string2double(const char *str, double &val) {
    return convert<double, double, strtod>(str, val);
}

} // namespace sk
