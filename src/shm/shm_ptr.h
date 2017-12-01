#ifndef SHM_PTR_H
#define SHM_PTR_H

#include <utility/utility.h>
#include <shm/detail/shm_address.h>

NS_BEGIN(sk)

template<typename T>
class shm_ptr {
public:
    shm_ptr() : addr_(nullptr) {}
    shm_ptr(std::nullptr_t) : addr_(nullptr) {}
    shm_ptr(const shm_ptr&) = default;

    explicit shm_ptr(const detail::shm_address& addr) : addr_(addr) {}

    shm_ptr& operator=(const shm_ptr&) = default;
    shm_ptr& operator=(std::nullptr_t) { addr_ = nullptr; return *this; }

    /*
     * the "explicit" keyword is not added here to make some convenient implicit
     * conversion available, however, we need to use the standard type traits
     * is_convertible<...> to confirm if a type U can be converted into T
     * the following operator=(...) function is similar to this
     */
    template<typename U,
             typename enable_if<std::is_convertible<U*, T*>::value>::type>
    shm_ptr(const shm_ptr<U>& ptr) : addr_(ptr.address()) {}

    template<typename U,
             typename enable_if<std::is_convertible<U*, T*>::value>::type>
    shm_ptr& operator=(const shm_ptr<U>& that) {
        addr_ = that.address();
        return *this;
    }

    const detail::shm_address& address() const { return addr_; }

    T *get() const {
        return addr_.as<T>();
    }

    T *operator->() const {
        T *ptr = get();
        sk_assert(ptr);
        return ptr;
    }

    typename std::add_lvalue_reference<T>::type operator*() const {
        T *ptr = get();
        sk_assert(ptr);
        return *ptr;
    }

    explicit operator bool() const { return !!addr_; }

private:
    detail::shm_address addr_;
};

template<typename T1, typename T2>
inline bool operator==(const shm_ptr<T1>& a, const shm_ptr<T2>& b) {
    return a.address() == b.address();
}

template<typename T1, typename T2>
inline bool operator!=(const shm_ptr<T1>& a, const shm_ptr<T2>& b) {
    return a.address() != b.address();
}

NS_END(sk)

#endif // SHM_PTR_H
