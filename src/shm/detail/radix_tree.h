#ifndef RADIX_TREE_H
#define RADIX_TREE_H

#include <shm/shm.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

/*
 * a 3-level radix tree
 */
template<typename V, size_t BITS0, size_t BITS1, size_t BITS2>
class radix_tree {
    static_assert(BITS0 + BITS1 + BITS2 <= sizeof(size_t) * CHAR_BIT, "too many bits");

public:
    const V *get(size_t key) const {
        // make sure the key does not overflow
        assert_retval(key >> (BITS0 + BITS1 + BITS2) == 0, nullptr);

        const size_t i0 = key >> (BITS1 + BITS2);
        const size_t i1 = (key >> BITS2) & (LENGTH1 - 1);
        const size_t i2 = key & (LENGTH2 - 1);

        if (!lv0_[i0]) return nullptr;

        const node_v1 *v1 = lv0_[i0].template as<node_v1>();
        if (!v1->lv1[i1]) return nullptr;

        const node_v2 *v2 = v1->lv1[i1].template as<node_v2>();
        return &(v2->lv2[i2]);
    }

    void set(size_t key, const V& v) {
        // make sure the key does not overflow
        assert_retnone(key >> (BITS0 + BITS1 + BITS2) == 0);

        const size_t i0 = key >> (BITS1 + BITS2);
        const size_t i1 = (key >> BITS2) & (LENGTH1 - 1);
        const size_t i2 = key & (LENGTH2 - 1);

        node_v1 *v1 = nullptr;
        node_v2 *v2 = nullptr;

        if (lv0_[i0]) {
            v1 = lv0_[i0].template as<node_v1>();
        } else {
            size_t size = sizeof(node_v1);
            lv0_[i0] = shm_allocate_metadata(&size);
            assert_retnone(lv0_[i0]);

            v1 = lv0_[i0].template as<node_v1>();
            // do not use memset, call constructor instead
            // memset(v1, 0x00, sizeof(*v1));
            new (v1) node_v1();
        }

        if (v1->lv1[i1]) {
            v2 = v1->lv1[i1].template as<node_v2>();
        } else {
            size_t size = sizeof(node_v2);
            v1->lv1[i1] = shm_allocate_metadata(&size);
            assert_retnone(v1->lv1[i1]);

            v2 = v1->lv1[i1].template as<node_v2>();
            // do not use memset, call constructor instead
            // memset(v2, 0x00, sizeof(*v2));
            new (v2) node_v2();
        }

        v2->lv2[i2] = v;
    }

private:
    static const size_t LENGTH0 = 1ULL << BITS0;
    static const size_t LENGTH1 = 1ULL << BITS1;
    static const size_t LENGTH2 = 1ULL << BITS2;

    struct node_v1 { shm_address lv1[LENGTH1]; };
    struct node_v2 { V           lv2[LENGTH2]; };

    shm_address lv0_[LENGTH0];
};

NS_END(detail)
NS_END(sk)

#endif // RADIX_TREE_H
