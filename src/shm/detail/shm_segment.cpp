#include "libsk.h"
#include "shm_segment.h"

namespace sk {
namespace detail {

int shm_segment::__create(key_t key, size_t size) {
    int shmid = shmget(key, size, 0666 | IPC_CREAT | IPC_EXCL);
    if (shmid != -1) {
        base_addr = shmat(shmid, nullptr, 0);
        assert_retval(base_addr, -1);

        this->shmid = shmid;
        return 0;
    }

    if (errno != EEXIST) {
        sk_error("shmget() failed, error<%s>.", strerror(errno));
        return -1;
    }

    shmid = shmget(key, 0, 0666);
    if (shmid == -1) {
        sk_error("shmget() failed, error<%s>.", strerror(errno));
        return -1;
    }

    int ret = shmctl(shmid, IPC_RMID, nullptr);
    if (ret != 0) {
        sk_error("free shm failure, error<%s>.", strerror(errno));
        return ret;
    }

    shmid = shmget(key, size, 0666 | IPC_CREAT | IPC_EXCL);
    if (shmid == -1) {
        sk_error("shmget() failed, error<%s>.", strerror(errno));
        return -1;
    }

    base_addr = shmat(shmid, nullptr, 0);
    assert_retval(base_addr, -1);

    this->shmid = shmid;
    return 0;
}

int shm_segment::__attach(key_t key) {
    int shmid = shmget(key, 0, 0666);
    if (shmid == -1) {
        sk_error("shmget() failed, error<%s>.", strerror(errno));
        return -1;
    }

    base_addr = shmat(shmid, nullptr, 0);
    assert_retval(base_addr, -1);

    this->shmid = shmid;
    return 0;
}

int shm_segment::init(key_t key, size_t size, bool resume) {
    return !resume ? __create(key, size) : __attach(key);
}

void shm_segment::fini() {
    if (shmid != -1) {
        int ret = shmctl(shmid, IPC_RMID, nullptr);
        if (ret != 0) {
            sk_error("free shm failure, error<%s>.", strerror(errno));
            return;
        }
    }

    base_addr = nullptr;
    shmid = -1;
}

} // namespace detail
} // namespace sk
