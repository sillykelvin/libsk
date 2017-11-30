#include <shm/shm.h>
#include <shm/detail/shm_address.h>

using namespace sk::detail;

shm_address shm_address::from_ptr(const void *ptr) {
    return shm_ptr2addr(ptr);
}

void *shm_address::get() {
    return shm_addr2ptr(*this);
}

const void *shm_address::get() const {
    return shm_addr2ptr(*this);
}
