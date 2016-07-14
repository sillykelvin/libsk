#include <errno.h>
#include "string_helper.h"

namespace sk {

template<typename T, T(*func)(const char *, char **, int)>
int string2type(const char *str, T& val) {
    if (!str) return -EINVAL;

    errno = 0;
    char *end = NULL;
    val = func(str, &end, 10);

    if (errno || *end != '\0' || end == str)
        return -EINVAL;

    return 0;
}

int string2s32(const char *str, s32& val) {
    return string2type<s32, strtol>(str, val);
}

int string2u32(const char *str, u32& val) {
    return string2type<s32, strtoul>(str, val);
}

int string2s64(const char *str, s64& val) {
    return string2type<s32, strtoll>(str, val);
}

int string2u64(const char *str, u64& val) {
    return string2type<s32, strtoull>(str, val);
}

} // namespace sk
