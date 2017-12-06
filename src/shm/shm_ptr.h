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
     * this constructor enables some convenient implicit conversions, however,
     * at least one constraint below must be passed to enable the conversion:
     *
     * 0. U* can be converted into T*, checked by std::is_convertible<...>
     * 1. U is void, checked by std::is_void<...>
     *
     * the following operator=(...) function is similar to this one
     */
    template<typename U, typename =
             typename enable_if<or_<std::is_void<U>,
                                    std::is_convertible<U*, T*>>::value>::type>
    shm_ptr(const shm_ptr<U>& ptr) : addr_(ptr.address()) {}

    template<typename U, typename =
             typename enable_if<or_<std::is_void<U>,
                                    std::is_convertible<U*, T*>>::value>::type>
    shm_ptr& operator=(const shm_ptr<U>& that) {
        addr_ = that.address();
        return *this;
    }

    template<typename U, U* = static_cast<U*>(static_cast<T*>(nullptr))>
    shm_ptr<U> cast() const { return shm_ptr<U>(addr_); }

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
