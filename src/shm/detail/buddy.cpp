#include "libsk.h"
#include "buddy.h"

sk::detail::buddy *sk::detail::buddy::create(void *addr, size_t mem_size, bool resume, u32 size, size_t unit_size) {
    assert_retval(addr, NULL);
    assert_retval(size >= 1, NULL);
    assert_retval(mem_size >= calc_size(size), NULL);

    u32 old_size = size;
    size = __fix_size(size);
    DBG("create buddy with size<%u>, after fixed<%u>.", old_size, size);

    buddy *self = cast_ptr(buddy, addr);

    if (resume) {
        assert_retval(self->size == size, NULL);
        assert_retval(self->unit_size == unit_size, NULL);
    } else {
        self->size = size;
        self->unit_size = unit_size;

        u32 node_size = size * 2;
        for (u32 i = 0; i < 2 * size - 1; ++i) {
            if (__power_of_two(i + 1))
                node_size /= 2;
            self->longest[i] = node_size;
        }
    }

    return self;
}

int sk::detail::buddy::init(char *pool) {
    assert_retval(pool, -EINVAL);
    assert_retval(pool + unit_size * size <= char_ptr(this)
                  || pool >= (char_ptr(this) + calc_size(size)), -EINVAL);

    check_retval(!this->pool, 0);

    this->pool = pool;

    return 0;
}

void *sk::detail::buddy::index2ptr(int unit_index) {
    assert_retval(unit_index >= 0 && static_cast<size_t>(unit_index) < size, NULL);

    return void_ptr(pool + unit_index * unit_size);
}

int sk::detail::buddy::malloc(size_t mem_size, int& unit_index) {
    u32 size = mem_size / unit_size;
    if (mem_size % unit_size != 0)
        size += 1;

    if (size <= 0)
        size = 1;

    size = __fix_size(size);

    u32 node_size;
    int index = 0;
    int offset = 0;

    if (longest[index] < size) {
        ERR("no more space :(");
        return -ENOMEM;
    }

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

    unit_index = offset;
    return 0;
}

void sk::detail::buddy::free(int offset) {
    assert_retnone(offset >= 0 && static_cast<u32>(offset) < size);

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
