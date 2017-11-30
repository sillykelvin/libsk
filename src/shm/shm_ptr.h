#ifndef SHM_PTR_H
#define SHM_PTR_H

#include <utility/utility.h>
#include <shm/detail/shm_address.h>

NS_BEGIN(sk)

NS_BEGIN(detail)
template<typename T> struct shm_ptr_traits { typedef T& reference; };
template<> struct shm_ptr_traits<void> { typedef void reference; };
NS_END(detail)

template<typename T>
class shm_ptr {
public:
    typedef T*                                            pointer;
    typedef const T*                                      const_pointer;
    typedef typename detail::shm_ptr_traits<T>::reference reference;
    typedef typename std::add_const<reference>::type      const_reference;

    shm_ptr() : addr_(nullptr) {}
    shm_ptr(std::nullptr_t) : addr_(nullptr) {}

    explicit shm_ptr(detail::shm_address addr) : addr_(addr) {}

    template<typename U>
    explicit shm_ptr(const shm_ptr<U>& ptr) : addr_(ptr.address()) {}

    const detail::shm_address& address() const { return addr_; }

    pointer get() {
        return addr_.as<T>();
    }

    const_pointer get() const {
        return addr_.as<T>();
    }

    shm_ptr& operator=(std::nullptr_t) {
        addr_ = nullptr;
        return *this;
    }

    template<typename U>
    shm_ptr& operator=(const shm_ptr<U>& that) {
        this->addr_ = that.address();
        return *this;
    }

    template<typename U>
    bool operator==(const shm_ptr<U>& that) const {
        return this->addr_ == that.address();
    }

    template<typename U>
    bool operator!=(const shm_ptr<U>& that) const {
        return this->addr_ != that.address();
    }

    template<typename U>
    bool operator<(const shm_ptr<U>& that) const {
        return this->get() < that.get();
    }

    pointer operator->() {
        pointer ptr = get();
        sk_assert(ptr);
        return ptr;
    }

    const_pointer operator->() const {
        const_pointer ptr = get();
        sk_assert(ptr);
        return ptr;
    }

    reference operator*() {
        pointer ptr = get();
        sk_assert(ptr);
        return *ptr;
    }

    const_reference operator*() const {
        const_pointer ptr = get();
        sk_assert(ptr);
        return *ptr;
    }

    explicit operator bool() const { return !!addr_; }

private:
    detail::shm_address addr_;
};

NS_END(sk)

#endif // SHM_PTR_H
