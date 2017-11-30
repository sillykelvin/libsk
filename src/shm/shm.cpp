#include <log/log.h>
#include <shm/shm.h>
#include <shm/shm_config.h>
#include <shm/detail/shm_mgr.h>
#include <shm/detail/shm_object.h>
#include <utility/assert_helper.h>

using namespace sk;
using namespace sk::detail;

static struct shm_context {
    shm_mgr *mgr;
    size_t mmap_size;
    char path[shm_config::MAX_PATH_SIZE];
} *ctx = nullptr;

shm_ptr<void> shm_malloc(size_t bytes) {
    return ctx->mgr->malloc(bytes);
}

void shm_free(shm_ptr<void> ptr) {
    ctx->mgr->free(ptr);
}

bool shm_has_singleton(int id) {
    return ctx->mgr->has_singleton(id);
}

shm_ptr<void> shm_get_singleton(int id, size_t bytes, bool *first_call) {
    return ctx->mgr->get_singleton(id, bytes, first_call);
}

void shm_free_singleton(int id) {
    return ctx->mgr->free_singleton(id);
}

size_map *shm_size_map() {
    return ctx->mgr->size_map();
}

page_heap *shm_page_heap() {
    return ctx->mgr->page_heap();
}

shm_address shm_allocate_metadata(size_t *bytes) {
    return ctx->mgr->allocate_metadata(bytes);
}

shm_address shm_allocate_userdata(size_t *bytes) {
    return ctx->mgr->allocate_userdata(bytes);
}

void *shm_addr2ptr(shm_address addr) {
    return ctx->mgr->addr2ptr(addr);
}

detail::shm_address shm_ptr2addr(const void *ptr) {
    return ctx->mgr->ptr2addr(ptr);
}

int shm_init(const char *basename, bool resume_mode) {
    if (ctx) {
        sk_warn("shm mgr already initialized.");
        return 0;
    }

    char path[shm_config::MAX_PATH_SIZE];
    snprintf(path, sizeof(path), "%s-mgr.mmap", basename);

    size_t total_bytes = sizeof(shm_mgr);
    int shmfd = resume_mode ? shm_object_attach(path, &total_bytes)
                            : shm_object_create(path, &total_bytes);
    if (shmfd == -1) return -errno;

    const size_t alignment = cast_size(sysconf(_SC_PAGESIZE));
    void *addr = shm_object_map(shmfd, &total_bytes, alignment);
    if (!addr) {
        int error = errno;
        close(shmfd);
        if (!resume_mode)
            shm_object_unlink(path);

        return -error;
    }

    int ret = 0;
    shm_mgr *mgr = cast_ptr(shm_mgr, addr);
    if (!resume_mode) {
        new (mgr) shm_mgr();
        ret = mgr->on_create(basename);
    } else {
        ret = mgr->on_resume(basename);
    }

    if (ret != 0) {
        close(shmfd);
        if (!resume_mode)
            shm_object_unlink(path);

        return ret;
    }

    ctx = cast_ptr(shm_context, malloc(sizeof(shm_context)));
    sk_assert(ctx);

    ctx->mgr = mgr;
    ctx->mmap_size = total_bytes;
    snprintf(ctx->path, sizeof(ctx->path), "%s", path);

    close(shmfd);
    return 0;
}

int shm_fini() {
    if (!ctx) return 0;

    ctx->mgr->~shm_mgr();
    shm_object_unmap(ctx->mgr, ctx->mmap_size);
    shm_object_unlink(ctx->path);

    free(ctx);
    ctx = nullptr;

    return 0;
}
