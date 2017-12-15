#ifndef FIXED_STRING_H
#define FIXED_STRING_H

#include <string>
#include <utility/assert_helper.h>

NS_BEGIN(sk)

template<size_t N>
class fixed_string {
public:
    ~fixed_string() { clear(); }
    fixed_string() : size_(0) { memory_[0] = 0; }

    explicit fixed_string(const char *str) {
        size_ = copy(memory_, N, str, strlen(str));
    }

    explicit fixed_string(const std::string& str) {
        size_ = copy(memory_, N, str.c_str(), str.length());
    }

    template<size_t M>
    explicit fixed_string(const fixed_string<M>& str) {
        size_ = copy(memory_, N, str.c_str(), str.length());
    }

    fixed_string& operator=(const char *str) {
        size_ = copy(memory_, N, str, strlen(str));
        return *this;
    }

    fixed_string& operator=(const std::string& str) {
        size_ = copy(memory_, N, str.c_str(), str.length());
        return *this;
    }

    template<size_t M>
    fixed_string& operator=(const fixed_string<M>& str) {
        size_ = copy(memory_, N, str.c_str(), str.length());
        return *this;
    }

public:
    bool operator==(const char *str) const {
        return eq(memory_, size_, str, strlen(str));
    }

    bool operator!=(const char *str) const {
        return !(*this == str);
    }

    bool operator==(const std::string& str) const {
        return eq(memory_, size_, str.c_str(), str.length());
    }

    bool operator!=(const std::string& str) const {
        return !(*this == str);
    }

    template<size_t M>
    bool operator==(const fixed_string<M>& str) const {
        return eq(memory_, size_, str.c_str(), str.length());
    }

    template<size_t M>
    bool operator!=(const fixed_string<M>& str) const {
        return !(*this == str);
    }

    bool operator<(const char *str) const {
        return compare(memory_, size_, str, strlen(str)) < 0;
    }

    bool operator<(const std::string& str) const {
        return compare(memory_, size_, str.c_str(), str.length()) < 0;
    }

    template<size_t M>
    bool operator<(const fixed_string<M>& str) const {
        return compare(memory_, size_, str.c_str(), str.length()) < 0;
    }

public:
    void clear() {
        size_ = 0;
        memory_[0] = 0;
    }

    size_t fill(char *array, size_t capacity) {
        return copy(array, capacity, memory_, size_);
    }

    std::string to_string() const {
        return std::string(memory_, size_);
    }

    const char *c_str() const { return memory_; }

    const char *data() const { return memory_; }

    size_t length()   const { return size_; }
    size_t capacity() const { return N; }

    // we have a '\0' at the end, so size_ is compared to N - 1 here
    bool full()  const { return size_ >= N - 1; }
    bool empty() const { return size_ <= 0; }

private:
    static size_t copy(char *dst, size_t dst_capacity,
                       const char *src, size_t src_length) {
        assert_retval(dst_capacity > 0, 0);

        size_t len = std::min(dst_capacity - 1, src_length);
        if (len > 0) memcpy(dst, src, len);

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
        if (length1 != length2) return false;

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

private:
    size_t size_;
    char memory_[sizeof(char) * N];
};

NS_END(sk)

#endif // FIXED_STRING_H
