#include "libsk.h"
#include "shm_segment.h"

int sk::detail::shm_segment::__create(key_t key, size_t size) {
    int shmid = shmget(key, size, 0666 | IPC_CREAT | IPC_EXCL);
    if (shmid != -1) {
        void *addr = shmat(shmid, NULL, 0);
        assert_retval(addr, -1);

        base_addr = char_ptr(addr);
        this->shmid = shmid;

        return 0;
    }

    if (errno != EEXIST) {
        ERR("shmget() failed, error<%s>.", strerror(errno));
        return -1;
    }

    shmid = shmget(key, 0, 0666);
    if (shmid == -1) {
        ERR("shmget() failed, error<%s>.", strerror(errno));
        return -1;
    }

    int ret = shmctl(shmid, IPC_RMID, NULL);
    if (ret != 0) {
        ERR("free shm failure, error<%s>.", strerror(errno));
        return ret;
    }

    shmid = shmget(key, size, 0666 | IPC_CREAT | IPC_EXCL);
    if (shmid == -1) {
        ERR("shmget() failed, error<%s>.", strerror(errno));
        return -1;
    }

    void *addr = shmat(shmid, NULL, 0);
    assert_retval(addr, -1);

    base_addr = char_ptr(addr);
    this->shmid = shmid;

    return 0;
}

int sk::detail::shm_segment::__attach(key_t key) {
    int shmid = shmget(key, 0, 0666);
    if (shmid == -1) {
        ERR("shmget() failed, error<%s>.", strerror(errno));
        return -1;
    }

    void *addr = shmat(shmid, NULL, 0);
    assert_retval(addr, -1);

    base_addr = char_ptr(addr);
    this->shmid = shmid;

    return 0;
}

int sk::detail::shm_segment::init(key_t key, size_t size, bool resume) {
    return !resume ? __create(key, size) : __attach(key);
}

void sk::detail::shm_segment::fini() {
    if (shmid != -1) {
        int ret = shmctl(shmid, IPC_RMID, NULL);
        if (ret != 0) {
            ERR("free shm failure, error<%s>.", strerror(errno));
            return;
        }
    }

    base_addr = NULL;
    shmid = -1;
}
