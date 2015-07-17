#include "libsk.h"
#include "buddy.h"

sk::detail::buddy *sk::detail::buddy::create(key_t key, bool resume, u32 size) {
    assert_retval(size >= 1, NULL);

    u32 old_size = size;
    size = __fix_size(size);
    DBG("create buddy with size<%u>, after fixed<%u>.", old_size, size);

    u32 leaf_count = 2 * size - 1;
    size_t shm_size = sizeof(buddy) + (leaf_count * sizeof(u32));

    sk::shm_seg seg;
    int ret = seg.init(key, shm_size, resume);
    if (ret != 0) {
        ERR("cannot create buddy, key<%d>, size<%lu>.", key, shm_size);
        return NULL;
    }

    /*
         * no matter in resume mode or not, we cast the entire memory block to buddy
         */
    buddy *self = NULL;
    self = static_cast<buddy *>(seg.address());

    if (!self) {
        ERR("memory error.");
        seg.free();
        return NULL;
    }

    if (!resume) {
        self->shmid = seg.shmid;
        self->size = size;

        u32 node_size = size * 2;
        for (u32 i = 0; i < 2 * size - 1; ++i) {
            if (__power_of_two(i + 1))
                node_size /= 2;
            self->longest[i] = node_size;
        }
    }

    return self;
}

int sk::detail::buddy::malloc(u32 size) {
    if (size <= 0)
        size = 1;

    size = __fix_size(size);

    u32 node_size;
    int index = 0;
    int offset = 0;

    if (longest[index] < size)
        return -1;

    for (node_size = this->size; node_size != size; node_size /= 2) {
        int left  = __left_leaf(index);
        int right = __right_leaf(index);
        int min   = left;
        int max   = right;

        if (longest[min] > longest[max]) {
            min = right;
            max = left;
        }

        if (longest[min] >= size)
            index = min;
        else
            index = max;
    }

    longest[index] = 0;

    offset = (index + 1) * node_size - this->size;

    while (index) {
        index = __parent(index);
        longest[index] = __max_child(index);
    }

    return offset;
}

void sk::detail::buddy::free(int offset) {
    assert_retnone(offset >= 0 && (u32) offset < size);

    u32 node_size = 1;
    int index = offset + size - 1;

    for (; longest[index]; index = __parent(index)) {
        node_size *= 2;
        if (index == 0)
            return;
    }

    longest[index] = node_size;

    while (index) {
        index = __parent(index);
        node_size *= 2;

        u32 left  = longest[__left_leaf(index)];
        u32 right = longest[__right_leaf(index)];

        if (left + right == node_size)
            longest[index] = node_size;
        else
            longest[index] = left > right ? left : right;
    }
}
