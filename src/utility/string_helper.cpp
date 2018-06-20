#include <sstream>
#include <stdarg.h>
#include <string.h>
#include <algorithm>
#include <utility/string_helper.h>

NS_BEGIN(sk)

int split_string(const char *str, char delim, std::vector<std::string>& vec) {
    if (!str) return -EINVAL;

    std::stringstream ss;
    ss.str(str);

    std::string item;
    while (std::getline(ss, item, delim))
        vec.push_back(item);

    return 0;
}

void replace_string(std::string& str, const std::string& src, const std::string& dst) {
    if (src.empty()) return;

    size_t pos = 0;
    while ((pos = str.find(src, pos)) != std::string::npos) {
        str.replace(pos, src.length(), dst);
        pos += dst.length();
    }
}

void fill_string(std::string& str, const char *fmt, ...) {
    static char buffer[8192];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    buffer[sizeof(buffer) - 1] = 0;

    str = buffer;
}

void trim_string(std::string& str) {
    str.erase(str.begin(),
              std::find_if(str.begin(), str.end(),
                           std::not1(std::ptr_fun<int, int>(std::isspace))));
    str.erase(std::find_if(str.rbegin(), str.rend(),
                           std::not1(std::ptr_fun<int, int>(std::isspace))).base(),
              str.end());
}

bool is_prefix(const std::string& prefix, const std::string& target) {
    const std::string *min = &prefix;
    const std::string *max = &target;
    if (min->length() > max->length()) {
        min = &target;
        max = &prefix;
    }

    auto it = std::mismatch(min->begin(), min->end(), max->begin());
    return it.first == min->end();
}

NS_END(sk)
