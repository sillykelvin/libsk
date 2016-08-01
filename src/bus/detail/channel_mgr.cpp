#include <errno.h>
#include "channel_mgr.h"
#include "shm/detail/shm_segment.h"
#include "utility/assert_helper.h"
#include "utility/config.h"
#include "common/lock_guard.h"

namespace sk {
namespace detail {

int channel::init(key_t shm_key, size_t node_size, size_t node_count, bool resume) {
    shm_segment seg;
    int ret = seg.init(shm_key, node_size * node_count, resume);
    if (ret != 0) return ret;

    if (resume) {
        assert_retval(shmid == seg.shmid, -1);
        assert_retval(this->shm_key == shm_key, -1);
        assert_retval(this->node_count == node_count, -1);
        assert_retval(addr, -1); // however, addr is an invalid address here

        addr = seg.address();
    } else {
        shmid = seg.shmid;
        this->shm_key = shm_key;
        this->node_count = node_count;
        assert_retval(node_count > 0, -1);

        read_pos = 0;
        write_pos = 0;

        addr = seg.address();
    }

    seg.release();
    return 0;
}

int channel::fini() {
    if (addr) {
        shmctl(shmid, IPC_RMID, 0);
        addr = NULL;
    }

    return 0;
}

int channel::push(const void *data, size_t length) {
    // TODO: implement this function
    return 0;
}

int channel::pop(void *data, size_t max_length, size_t *real_length) {
    // TODO: implement this function
    return 0;
}

bool channel::empty() const {
    // TODO: implement this function
    return 0;
}

int channel_mgr::init(int shmid, size_t node_size, size_t node_count, bool resume) {
    if (resume) {
        assert_retval(magic == MAGIC, -1);
        assert_retval(this->shmid == shmid, -1);
        assert_retval(this->node_count == node_count, -1);
        assert_retval(this->node_size == node_size, -1);

        // it's better to lock the operation, to ensure there is no other
        // processes doing registration
        lock_guard(lock);
        for (size_t i = 0; i < channel_count; ++i) {
            detail::channel& c = channel_list[i];

            // because this is resume mode, so we just use shm_key stored in channel
            int ret = c.init(c.shm_key, node_size, node_count, resume);
            if (ret != 0) {
                // TODO: error log here
                return ret;
            }
        }
    } else {
        this->shmid = shmid;
        this->node_count = node_count;
        assert_retval(node_count > 0, -1);
        this->node_size = node_size;
        assert_retval(node_size > 0, -1);
        assert_retval(node_size & (node_size - 1) == 0, -1);

        node_size_shift = 0;
        while (node_size > 1) {
            node_size = node_size >> 1;
            ++node_size_shift;
        }

        lock.init();
        channel_count = 0;
        memset(channel_list, 0x00, sizeof(channel_list));

        // we set magic at the last, to ensure all fields are initialized
        magic = MAGIC;
    }

    return 0;
}

int channel_mgr::fini() {
    lock_guard(lock);
    for (size_t i = 0; i < channel_count; ++i) {
        detail::channel& c = channel_list[i];
        int ret = c.fini();
        if (ret != 0) {
            // TODO: error log here
        }
    }

    // ignore those fini failed channels
    return 0;
}

size_t channel_mgr::calc_node_count(size_t length) {
    if (length == 0) return 1;

    return ((length - 1) / node_size) + 1;
}

int channel_mgr::register_channel(key_t shm_key, size_t& handle) {
    if (magic != MAGIC) {
        error("channel mgr has not been initialized.");
        return -EINVAL;
    }

    size_t idx = -1;

    {
        lock_guard(lock);
        for (size_t i = 0; i < channel_count; ++i) {
            if (channel_list[i].shm_key == shm_key) {
                info("channel already exists, key<%d>.", shm_key);
                handle = i;
                return 0;
            }
        }

        if (channel_count >= array_len(channel_list)) {
            error("channel list is full.");
            return -ENOMEM;
        }

        idx = channel_count;
        ++channel_count;
    }

    assert_retval(idx != -1, -1);

    int ret = channel_list[idx].init(shm_key, node_size, node_count, false);
    if (ret != 0) {
        error("channel initialize error, ret<%d>, key<%d>.", ert, shm_key);
        return ret;
    }

    handle = idx;
    return 0;
}

} // namespace detail
} // namespace sk
