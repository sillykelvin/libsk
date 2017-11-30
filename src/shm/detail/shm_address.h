#ifndef SHM_ADDRESS_H
#define SHM_ADDRESS_H

#include <shm/shm_config.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

using block_t  = size_t;
using page_t   = size_t;
using offset_t = size_t;
using serial_t = size_t;

class shm_address {
public:
    static shm_address from_ptr(void *ptr);

public:
    // this constructor constructs an empty address
    shm_address() : block_(0), offset_(0), serial_(0) {}

    // this constructor constructs an empty address, do NOT add the "explicit" keyword
    shm_address(std::nullptr_t) : block_(0), offset_(0), serial_(0) {}

    // this constructor constructs an address represents a shm block
    explicit shm_address(block_t block)
        : block_(block), offset_(0), serial_(shm_config::SPECIAL_SERIAL) {}

    // this constructor constructs an address represents an offset inside a shm block,
    // it's used when allocating internal needed objects, which does not have a serial
    explicit shm_address(block_t block, offset_t offset)
        : block_(block), offset_(offset), serial_(shm_config::SPECIAL_SERIAL) {}

    // this constructor constructs an address represents an offset inside a shm block
    // with a specified serial, it's used when allocating objects needed by the user
    explicit shm_address(block_t block, offset_t offset, serial_t serial)
        : block_(block), offset_(offset), serial_(serial) {
        // 0 represents nullptr, the special serial represents internal objects, so
        // we avoid the two values
        sk_assert(serial != 0 && serial != shm_config::SPECIAL_SERIAL);
    }

public:
    template<typename T>
    const T *as() const {
        return static_cast<const T*>(get());
    }

    template<typename T>
    T *as() {
        return static_cast<T*>(get());
    }

    const void *get() const;
    void *get();

    block_t  block()  const { return block_;  }
    offset_t offset() const { return offset_; }
    serial_t serial() const { return serial_; }

public:
    explicit operator bool() const { return serial_ != 0; }

    shm_address& operator=(std::nullptr_t) {
        block_  = 0;
        offset_ = 0;
        serial_ = 0;

        return *this;
    }

    bool operator==(const shm_address& that) const {
        return this->block_  == that.block_  &&
               this->offset_ == that.offset_ &&
               this->serial_ == that.serial_;
    }

    bool operator!=(const shm_address& that) const {
        return this->block_  != that.block_  ||
               this->offset_ != that.offset_ ||
               this->serial_ != that.serial_;
    }

private:
    size_t block_  : shm_config::MAX_BLOCK_BITS;
    size_t offset_ : shm_config::MAX_OFFSET_BITS;
    size_t serial_ : shm_config::MAX_SERIAL_BITS;
};
static_assert(sizeof(shm_address) == sizeof(size_t), "invalid shm_address");
static_assert(std::is_standard_layout<shm_address>::value, "invalid shm_address");

NS_END(detail)
NS_END(sk)

#endif // SHM_ADDRESS_H
