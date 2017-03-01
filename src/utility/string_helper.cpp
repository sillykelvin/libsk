#include <sstream>
#include <stdarg.h>
#include <algorithm>
#include <string.h>
#include <openssl/md5.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include "string_helper.h"

namespace sk {

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

bool is_prefix(const std::string& str1, const std::string& str2) {
    const std::string *min = &str1;
    const std::string *max = &str2;
    if (min->length() > max->length()) {
        min = &str2;
        max = &str1;
    }

    auto it = std::mismatch(min->begin(), min->end(), max->begin());
    return it.first == min->end();
}

std::string base64_decode(const std::string& str) {
    static char buf[512];

    bool allocated = false;
    char *buffer = buf;
    if (str.length() > array_len(buf)) {
        buffer = (char *) malloc(str.length());
        allocated = true;
    }

    BIO *b64  = BIO_new(BIO_f_base64());
    BIO *bmem = BIO_new_mem_buf(str.data(), str.length());
    bmem = BIO_push(b64, bmem);

    BIO_set_flags(bmem, BIO_FLAGS_BASE64_NO_NL);
    int out_len = BIO_read(bmem, buffer, str.length());

    std::string output(buffer, out_len);
    BIO_free_all(bmem);

    if (allocated)
        free(buffer);

    return output;
}

std::string md5_string(const std::string& str) {
    unsigned char md[16] = {0};
    char tmp[3] = {0};
    char buf[33] = {0};

    MD5((const unsigned char*) str.c_str(), str.size(), md);

    for (int i = 0; i < 16; ++i) {
        sprintf(tmp, "%2.2x", md[i]);
        strcat(buf, tmp);
    }

    return buf;
}

} // namespace sk
