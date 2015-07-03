#ifndef DETAIL_STACK_H
#define DETAIL_STACK_H

namespace sk {
namespace detail {

template<typename T>
struct stack {
    int  shmid;
    int  pos;
    int  total_count;
    char data[0];

    static stack *create(key_t key, bool resume, int max_count) {
        size_t data_size = max_count * sizeof(T);
        size_t shm_size = sizeof(stack) + data_size;
        sk::shm_seg seg;
        int ret = seg.init(key, shm_size, resume);
        if (ret != 0) {
            ERR("cannot create list, key<%d>, size<%lu>.", key, shm_size);
            return NULL;
        }

        /*
         * no matter in resume mode or not, we cast the entire memory block to stack
         */
        stack *self = NULL;
        self = static_cast<stack *>(seg.address());

        if (!self) {
            ERR("memory error.");
            seg.free();
            return NULL;
        }

        if (!resume) {
            self->shmid = seg.shmid;
            self->pos = 0;
            self->total_count = max_count;
            memset(self->data, 0x00, data_size);
        }

        return self;
    }

    bool empty() const {
        return pos <= 0;
    }

    bool full() const {
        return pos >= total_count;
    }

    int push(const T& t) {
        if (full())
            return -ENOMEM;

        T *ret = emplace();
        assert_retval(ret, -EINVAL);

        *ret = t;

        return 0;
    }

    T *emplace() {
        if (full())
            return NULL;

        T *t = static_cast<T *>(static_cast<void *>(data + sizeof(T) * pos));
        ++pos;

        return t;
    }

    T *pop() {
        if (empty())
            return NULL;

        --pos;
        return static_cast<T *>(static_cast<void *>(data + sizeof(T) * pos));
    }
};

} // namespace detail
} // namespace sk

#endif // DETAIL_STACK_H
