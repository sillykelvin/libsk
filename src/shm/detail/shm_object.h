#ifndef SHM_OBJECT_H
#define SHM_OBJECT_H

#include <utility/types.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

int shm_object_create(const char *path, size_t *size);
int shm_object_attach(const char *path, size_t *size);
int shm_object_resize(const char *path, size_t *size);

void *shm_object_map(int shmfd, size_t *mmap_size, size_t alignment);
void shm_object_unmap(void *addr, size_t mmap_size);

void shm_object_unlink(const char *path);

NS_END(detail)
NS_END(sk)

#endif // SHM_OBJECT_H
