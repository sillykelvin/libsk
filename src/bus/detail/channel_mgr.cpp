#include <errno.h>
#include "channel_mgr.h"
#include "shm/detail/shm_segment.h"
#include "utility/assert_helper.h"
#include "utility/config.h"
#include "common/lock_guard.h"
#include "bus/detail/channel.h"

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

int channel_mgr::init(int shmid, size_t shm_size, bool resume) {
    if (resume) {
        assert_retval(this->magic == MAGIC, -1);
        assert_retval(this->shmid == shmid, -1);
        assert_retval(this->shm_size == shm_size, -1);
    } else {
        this->shmid = shmid;
        this->shm_size = shm_size;
        // TODO: may do some alignment job here
        this->used_size = sizeof(channel_mgr);

        lock.init();
        descriptor_count = 0;
        memset(descriptors, 0x00, sizeof(descriptors));

        // we set magic at the last, to ensure all fields are initialized
        this->magic = MAGIC;
    }

    return 0;
}

int channel_mgr::register_channel(int busid, size_t node_size, size_t node_count, int& fd) {
    if (magic != MAGIC) {
        error("channel mgr has not been initialized.");
        return -EINVAL;
    }

    if (node_size & (node_size - 1) != 0) {
        error("node size %lu must be 2 ^ N.", node_size);
        return -EINVAL;
    }

    fd = -1;
    channel_descriptor *desc = NULL;

    {
        lock_guard(lock);
        for (int i = 0; i < descriptor_count; ++i) {
            if (descriptors[i].owner == busid) {
                info("channel already exists, bus<%x>.", busid);
                fd = i;
                return 0;
            }
        }

        size_t channel_size = node_size * node_count;
        assert_retval(channel_size > 0, -1);

        size_t left_size = 0;
        if (shm_size > used_size)
            left_size = shm_size - used_size;

        // mulitple 2 here because there are two channels, one for read & another for write
        if (left_size < channel_size * 2) {
            error("left size %lu is not enough, required: %lu.", left_size, channel_size * 2);
            return -ENOMEM;
        }

        fd = descriptor_count++;
        desc = &descriptors[fd];
        desc->owner = busid;
        desc->r_offset = used_size;
        used_size += channel_size;
        desc->w_offset = used_size;
        used_size += channel_size;
    }

    int ret = 0;

    channel *rc = cast_ptr(channel, char_ptr(this) + desc->r_offset);
    ret = rc->init(node_size, node_count);
    if (ret != 0) {
        error("failed to init read channel, bus id<%x>, ret<%d>.", busid, ret);
        return ret;
    }

    channel *wc = cast_ptr(channel, char_ptr(this) + desc->w_offset);
    ret = wc->init(node_size, node_count);
    if (ret != 0) {
        error("failed to init write channel, bus id<%x>, ret<%d>.", busid, ret);
        return ret;
    }

    info("new channel, fd<%d>, owner<%x>, read offset<%lu>, write offset<%lu>.",
         fd, desc->owner, desc->r_offset, desc->w_offset);

    return 0;
}

channel *channel_mgr::get_read_channel(int fd) {
    assert_retval(fd >= 0 && fd < descriptor_count, NULL);

    channel_descriptor& desc = descriptors[fd];
    channel *rc = cast_ptr(channel, char_ptr(this) + desc->r_offset);
    assert_retval(rc->magic == MAGIC, NULL);

    return rc;
}

channel *channel_mgr::get_write_channel(int fd) {
    assert_retval(fd >= 0 && fd < descriptor_count, NULL);

    channel_descriptor& desc = descriptors[fd];
    channel *wc = cast_ptr(channel, char_ptr(this) + desc->w_offset);
    assert_retval(wc->magic == MAGIC, NULL);

    return wc;
}

} // namespace detail
} // namespace sk
