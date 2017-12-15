#ifndef FIXED_VECTOR_H
#define FIXED_VECTOR_H

#include <algorithm>
#include <utility/assert_helper.h>

NS_BEGIN(sk)

template<typename T, size_t N>
class fixed_vector {
public:
    typedef T*       iterator;
    typedef const T* const_iterator;

public:
    ~fixed_vector() { clear(); }
    fixed_vector() : size_(0), peak_size_(0) {}

    fixed_vector(const fixed_vector& vector) : size_(0), peak_size_(0) {
        for (const_iterator it = vector.begin(), end = vector.end(); it != end; ++it)
            emplace(*it);
    }

    fixed_vector& operator=(const fixed_vector& vector) {
        if (this == &vector) return *this;

        clear();

        for (const_iterator it = vector.begin(), end = vector.end(); it != end; ++it)
            emplace(*it);

        return *this;
    }

public:
    void clear() {
        for (iterator it = this->begin(), end = this->end(); it != end; ++it)
            it->~T();

        size_ = 0;
    }

    void fill(size_t n, const T& value) {
        assert_retnone(n <= capacity());

        if (n >= size()) {
            std::fill(begin(), end(), value);
            size_t left = n - size();
            while (left-- > 0)
                emplace(value);

            return;
        }

        std::fill(begin(), at(n), value);
        for (size_t i = size_ - 1; i >= n; --i)
            erase_at(i);
    }

    iterator at(size_t index) {
        assert_retval(index < size_, end());
        return begin() + index;
    }

    const_iterator at(size_t index) const {
        assert_retval(index < size_, end());
        return begin() + index;
    }

    void erase_at(size_t index) {
        assert_retnone(index < size_);

        if (at(index) + 1 != end())
            std::copy(at(index) + 1, end(), at(index));

        at(size_ - 1)->~T();
        --size_;
    }

    void erase(iterator it) {
        assert_retnone(it != end());
        size_t index = it - begin();
        erase_at(index);
    }

    void erase(const T& value) {
        iterator it = find(value);
        if (it == end()) return;

        erase(it);
    }

    template<typename Pred>
    void erase_if(Pred p) {
        iterator it = find_if(p);
        if (it == end()) return;

        erase(it);
    }

    template<typename... Args>
    T* emplace(Args&&... args) {
        if (full()) return nullptr;

        T *t = at(size_++);
        new (t) T(std::forward<Args>(args)...);

        if (size_ > peak_size_)
            peak_size_ = size_;

        return t;
    }

    T& operator[](size_t index) {
        return *at(index);
    }

    const T& operator[](size_t index) const {
        return *at(index);
    }

    iterator find(const T& value) {
        for (iterator it = this->begin(), end = this->end(); it != end; ++it) {
            if (*it == value) return it;
        }

        return this->end();
    }

    const_iterator find(const T& value) const {
        for (const_iterator it = this->begin(), end = this->end(); it != end; ++it) {
            if (*it == value) return it;
        }

        return this->end();
    }

    template<typename Pred>
    iterator find_if(Pred p) {
        for (iterator it = this->begin(), end = this->end(); it != end; ++it) {
            if (p(*it)) return it;
        }

        return this->end();
    }

    template<typename Pred>
    const_iterator find_if(Pred p) const {
        for (const_iterator it = this->begin(), end = this->end(); it != end; ++it) {
            if (p(*it)) return it;
        }

        return this->end();
    }

    iterator begin() { return cast_ptr(T, memory_); }
    iterator end()   { return cast_ptr(T, memory_) + size_; }
    const_iterator begin() const { return reinterpret_cast<const T*>(memory_); }
    const_iterator end()   const { return reinterpret_cast<const T*>(memory_) + size_; }

    bool full()  const { return size_ >= N; }
    bool empty() const { return size_ <= 0; }
    size_t size()     const { return size_; }
    size_t capacity() const { return N; }
    size_t peak_size() const { return peak_size_; }

private:
    size_t size_;
    size_t peak_size_;
    char memory_[sizeof(T) * N];
};

NS_END(sk)

#endif // FIXED_VECTOR_H
