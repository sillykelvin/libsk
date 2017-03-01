#include <stdarg.h>
#include "error_info.h"

static int error_code;
static std::string error_info;

namespace sk {

void set_last_error(int code, const char *fmt, ...) {
    static char buffer[8192];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    buffer[sizeof(buffer) - 1] = 0;

    error_code = code;
    error_info = buffer;
}

void set_last_error(int code, const std::string& info) {
    error_code = code;
    error_info = info;
}

int last_error_code() {
    return error_code;
}

const std::string& last_error_info() {
    return error_info;
}

} // namespace sk
