#include <errno.h>
#include "string_helper.h"

namespace sk {

int string2s32(const char *str, s32& val) {
    if (!str) return -EINVAL;

    errno = 0;
    char *end = NULL;
    val = strtol(str, &end, 10);

    if (errno || *end != '\0' || end == str)
        return -EINVAL;

    return 0;
}

int string2u32(const char *str, u32& val) {
    if (!str) return -EINVAL;

    errno = 0;
    char *end = NULL;
    val = strtoul(str, &end, 10);

    if (errno || *end != '\0' || end == str)
        return -EINVAL;

    return 0;
}

int string2s64(const char *str, s64& val) {
    if (!str) return -EINVAL;

    errno = 0;
    char *end = NULL;
    val = strtoll(str, &end, 10);

    if (errno || *end != '\0' || end == str)
        return -EINVAL;

    return 0;
}

int string2u64(const char *str, u64& val) {
    if (!str) return -EINVAL;

    errno = 0;
    char *end = NULL;
    val = strtoull(str, &end, 10);

    if (errno || *end != '\0' || end == str)
        return -EINVAL;

    return 0;
}

} // namespace sk
