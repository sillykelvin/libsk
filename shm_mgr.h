#ifndef SHM_MGR_H
#define SHM_MGR_H

#include "types.h"
#incldue "shm_seg.h"
#include <stddef.h>

namespace sk {
namespace detail {

template<typename K, typename V>
struct hash_node {
    K k;
    V v;

    int next;
};

template<typename K, typename V>
struct hash {
    typedef hash_node<K, V> node;

    int max_node_count;
    int curr_node_count;
    int hash_size;
    int free_node_head;
    int *buckets;
    node *nodes;

    hash() : max_node_count(0), curr_node_count(0), hash_size(0), free_node_head(-1), buckets(NULL), nodes(NULL) {}
    ~hash() {}

    static hash *create(key_t key, bool resume, int max_node_count, int hash_size = 0) {
        if (hash_size == 0)
            hash_size = max_node_count;

        size_t shm_size = sizeof(hash) + max_node_count * sizeof(node) + hash_size * sizeof(int);

        sk::shm_seg seg;
        int ret = seg.init(key, shm_size, resume);
        if (ret != 0) {
            ERR("cannot create hash map, key<%d>, size<%lu>.", key, shm_size);
            return NULL;
        }

        hash *self = NULL;
        if (resume) {
            self = static_cast<hash *>(seg.address());
        } else {
            self = static_cast<hash *>(seg.malloc(sizeof(hash)));
        }

        if (!self) {
            ERR("memory error.");
            seg.free();
            return NULL;
        }

        if (resume) {
            char *base_addr = static_cast<char *>(seg.address());
            self->buckets = static_cast<int *>(base_addr + sizeof(hash));
            self->nodes = static_cast<node *>(base_addr + sizeof(hash) + sizeof(int) * hash_size);
        } else {
            self->buckets = static_cast<int *>(seg.malloc(sizeof(int) * hash_size));
            if (!self->buckets) {
                ERR("memory error.");
                seg.free();
                return NULL;
            }

            self->nodes = static_cast<node *>(seg.malloc(sizeof(node) * max_node_count));
            if (!self->nodes) {
                ERR("memory error.");
                seg.free();
                return NULL;
            }

            self->max_node_count = max_node_count;
            self->curr_node_count = 0;
            self->hash_size = hash_size;

            for (int i = 0; i < hash_size; ++i)
                self->buckets[i] = IDX_NULL;

            for (int i = 0; i < max_node_count; ++i)
                self->nodes[i].next = i + 1;

            self->nodes[max_node_count - 1] = IDX_NULL;

            self->free_node_head = 0;
        }

        return self;
    }
};

} // namespace detail
} // namespace sk

#endif // SHM_MGR_H
