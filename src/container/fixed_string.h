#ifndef FIXED_STRING_H
#define FIXED_STRING_H

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

    template<size_t M>
    explicit fixed_string(const fixed_string<M>& str) {
        used_count = copy(this->memory, N, str.memory, str.length());
    }

    fixed_string& operator=(const char *str) {
        used_count = copy(this->memory, N, str, strlen(str));
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

    template<size_t M>
    bool operator==(const fixed_string<M>& str) const {
        return eq(memory, used_count, str.memory, str.used_count);
    }

    template<size_t M>
    bool operator!=(const fixed_string<M>& str) const {
        return !(this->operator ==(str));
    }

    void clear() {
        used_count = 0;
        memory[0] = '\0';
    }

    size_t fill(char *array, size_t capacity) {
        return copy(array, capacity, memory, used_count);
    }

    const char *c_str() const { return memory; }

    size_t length()   const { return used_count; }
    size_t capacity() const { return N; }

    bool full()  const { return used_count >= N; }
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
};

} // namespace sk

#endif // FIXED_STRING_H
