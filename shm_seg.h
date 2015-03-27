#ifndef SHM_SEG_H
#define SHM_SEG_H

#include <sys/shm.h>
#include <errno.h>
#include <stddef.h>
#include "log.h"

namespace sk {

struct shm_seg {
    char *base_addr;
    char *free_addr;
    size_t free_size;

    shm_seg() : base_addr(NULL), free_addr(NULL), free_size(0) {}
    ~shm_seg() {}

    int init(key_t key, size_t size) {
        int shmid = shmget(key, size, 0666 | IPC_CREAT | IPC_EXCL);
        if (shmid == -1) {
            if (errno != EEXIST) {
                ERR("shmget() failed, error<%d>.", errno);
                return -1;
            }

            shmid = shmget(key, size, 0666);

            if (shmid == -1) {
                ERR("shmget() failed, error<%d>.", errno);
                return -1;
            }
        }

        void *addr = shmat(shmid, NULL, 0);
        if (!addr) {
            ERR("shmat() failed.");
            return -1;
        }

        base_addr = static_cast<char *>(addr);
        free_addr = base_addr;
        free_size = size;

        return 0;
    }

    void *malloc(size_t size) {
        if (size > free_size)
            return NULL;

        void *ret = static_cast<void *>(free_addr);

        free_addr += size;
        free_size -= size;

        return ret;
    }
};

} // namespace sk

#endif // SHM_SEG_H
