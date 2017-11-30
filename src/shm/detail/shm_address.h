#ifndef SHM_ADDRESS_H
#define SHM_ADDRESS_H

#include <shm/shm_config.h>
#include <utility/assert_helper.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

class shm_address {
public:
    static shm_address from_ptr(const void *ptr);

public:
    shm_address() : addr_(0) {}
    shm_address(std::nullptr_t) : addr_(0) {}

    shm_address(shm_serial_t serial, shm_offset_t offset) {
        sk_assert((serial >> shm_config::SERIAL_BITS)  == 0);
        sk_assert((offset >> shm_config::ADDRESS_BITS) == 0);
        addr_ = (serial << shm_config::ADDRESS_BITS) | (offset & shm_config::ADDRESS_MASK);
    }

    u64 as_u64() const { return addr_; }
    shm_serial_t serial() const { return addr_ >> shm_config::ADDRESS_BITS; }
    shm_offset_t offset() const { return addr_ &  shm_config::ADDRESS_MASK; }

    void *get();
    const void *get() const;

    template<typename T> T *as() { return static_cast<T*>(get()); }
    template<typename T> const T *as() const { return static_cast<const T*>(get()); }

    // shm_address& operator=(std::nullptr_t) { addr_ = 0; }

    bool operator==(const shm_address& that) const { return addr_ == that.addr_; }
    bool operator!=(const shm_address& that) const { return addr_ != that.addr_; }

    explicit operator bool() const { return serial() != 0; }

private:
    // 16 bits serial + 48 bits offset
    u64 addr_;
};

NS_END(detail)
NS_END(sk)

#endif // SHM_ADDRESS_H
