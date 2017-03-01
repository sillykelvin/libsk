#ifndef FIXED_STRING_H
#define FIXED_STRING_H

#include <string>
#include "utility/types.h"
#include "utility/utility.h"
#include "utility/assert_helper.h"

namespace sk {

template<size_t N>
struct fixed_string {
    size_t used_count;
    char memory[sizeof(char) * N];

    fixed_string() : used_count(0) { memory[0] = '\0'; }
    ~fixed_string() { clear(); }

    explicit fixed_string(const char *str) {
        used_count = copy(this->memory, N, str, strlen(str));
    }

    explicit fixed_string(const std::string& str) {
        used_count = copy(this->memory, N, str.c_str(), str.length());
    }

    template<size_t M>
    explicit fixed_string(const fixed_string<M>& str) {
        used_count = copy(this->memory, N, str.memory, str.length());
    }

    fixed_string& operator=(const char *str) {
        used_count = copy(this->memory, N, str, strlen(str));
        return *this;
    }

    fixed_string& operator=(const std::string& str) {
        used_count = copy(this->memory, N, str.c_str(), str.length());
        return *this;
    }

    template<size_t M>
    fixed_string& operator=(const fixed_string<M>& str) {
        used_count = copy(this->memory, N, str.memory, str.length());
        return *this;
    }

    bool operator==(const char *str) const {
        return eq(memory, used_count, str, strlen(str));
    }

    bool operator!=(const char *str) const {
        return !(this->operator ==(str));
    }

    bool operator==(const std::string& str) const {
        return eq(memory, used_count, str.c_str(), str.length());
    }

    bool operator!=(const std::string& str) const {
        return !(this->operator ==(str));
    }

    template<size_t M>
    bool operator==(const fixed_string<M>& str) const {
        return eq(memory, used_count, str.memory, str.used_count);
    }

    template<size_t M>
    bool operator!=(const fixed_string<M>& str) const {
        return !(this->operator ==(str));
    }

    bool operator<(const char *str) const {
        return compare(data(), length(), str, strlen(str)) < 0;
    }

    bool operator<(const std::string& str) const {
        return compare(data(), length(), str.data(), str.length()) < 0;
    }

    template<size_t M>
    bool operator<(const fixed_string<M>& str) const {
        return compare(data(), length(), str.data(), str.length()) < 0;
    }

    void clear() {
        used_count = 0;
        memory[0] = '\0';
    }

    size_t fill(char *array, size_t capacity) {
        return copy(array, capacity, memory, used_count);
    }

    std::string to_string() const {
        return std::string(memory, used_count);
    }

    const char *c_str() const { return memory; }

    const char *data() const { return memory; }

    size_t length()   const { return used_count; }
    size_t capacity() const { return N; }

    // we have a '\0' at the end, so used_count is compared to N - 1 here
    bool full()  const { return used_count >= N - 1; }
    bool empty() const { return used_count <= 0; }

    static size_t copy(char *dst, size_t dst_capacity,
                       const char *src, size_t src_length) {
        assert_retval(dst_capacity > 0, 0);

        size_t len = min(dst_capacity - 1, src_length);
        if (len > 0)
            memcpy(dst, src, len);

        dst[len] = '\0';
        return len;
    }

    static size_t strlen(const char *str) {
        if (!str) return 0;

        size_t i = 0;
        while (str[i] != '\0') ++i;

        return i;
    }

    static bool eq(const char *str1, size_t length1,
                   const char *str2, size_t length2) {
        if (length1 != length2)
            return false;

        for (size_t i = 0; i < length1; ++i)
            if (str1[i] != str2[i])
                return false;

        return true;
    }

    static int compare(const char *str1, size_t length1,
                       const char *str2, size_t length2) {
        int ret = strncmp(str1, str2, std::min(length1, length2));
        if (ret != 0) return ret;

        if (length1 < length2)
            return -1;

        if (length1 > length2)
            return 1;

        return 0;
    }
};

} // namespace sk

#endif // FIXED_STRING_H
