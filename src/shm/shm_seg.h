#ifndef SHM_SEG_H
#define SHM_SEG_H

namespace sk {

struct shm_seg {
    char *base_addr;
    char *free_addr;
    size_t free_size;
    int shmid;
    bool owner;

    shm_seg() : base_addr(NULL), free_addr(NULL), free_size(0), shmid(-1), owner(true) {}
    ~shm_seg() { if (owner && shmid != -1) free(); }

    int __create(key_t key, size_t size) {
        int shmid = shmget(key, size, 0666 | IPC_CREAT | IPC_EXCL);
        if (shmid != -1) {
            void *addr = shmat(shmid, NULL, 0);
            assert_retval(addr, -1);

            base_addr = char_ptr(addr);
            free_addr = base_addr;
            free_size = size;
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
        free_addr = base_addr;
        free_size = size;
        this->shmid = shmid;

        return 0;
    }

    int __attach(key_t key) {
        int shmid = shmget(key, 0, 0666);
        if (shmid == -1) {
            ERR("shmget() failed, error<%s>.", strerror(errno));
            return -1;
        }

        void *addr = shmat(shmid, NULL, 0);
        assert_retval(addr, -1);

        /*
         * Under resume mode, we assume that the entire memory segment is
         * allocated, so malloc() will always fail.
         */
        base_addr = char_ptr(addr);
        free_addr = NULL;
        free_size = 0;
        this->shmid = shmid;

        return 0;
    }

    int init(key_t key, size_t size, bool resume) {
        return !resume ? __create(key, size) : __attach(key);
    }

    /*
     * if the shared memory is released from this segment, it
     * will NOT be destroyed when the segment is destructed.
     */
    void release() {
        owner = false;
    }

    void *address() {
        return void_ptr(base_addr);
    }

    void *malloc(size_t size) {
        if (size > free_size)
            return NULL;

        void *ret = void_ptr(free_addr);

        free_addr += size;
        free_size -= size;

        return ret;
    }

    void free() {
        int ret = shmctl(shmid, IPC_RMID, NULL);
        if (ret != 0)
            ERR("free shm failure, error<%s>.", strerror(errno));
    }
};

} // namespace sk

#endif // SHM_SEG_H
