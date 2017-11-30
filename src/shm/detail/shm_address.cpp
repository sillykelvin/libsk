#include <shm/shm_mgr.h>
#include <utility/math_helper.h>

using namespace sk::detail;

shm_address shm_address::from_ptr(void *ptr) {
    return shm_mgr::ptr2addr(ptr);
}

const void *shm_address::get() const {
    return shm_mgr::addr2ptr(*this);
}

void *shm_address::get() {
    return shm_mgr::addr2ptr(*this);
}
