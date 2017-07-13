#include <signal.h>
#include <string.h>
#include <errno.h>
#include "channel_mgr.h"
#include "bus/bus.h"
#include "log/log.h"
#include "utility/math_helper.h"
#include "shm/detail/shm_segment.h"
#include "utility/assert_helper.h"
#include "utility/config.h"
#include "common/lock_guard.h"
#include "bus/detail/channel.h"

NS_BEGIN(sk)
NS_BEGIN(detail)

static int notify_channel_change(int bus_pid, int fd) {
    sigval value;
    memset(&value, 0x00, sizeof(value));
    value.sival_int = fd;
    return sigqueue(bus_pid, sk::bus::BUS_REGISTRATION_SIGNO, value);
}

int channel_mgr::init(int shmid, size_t shm_size, bool resume) {
    if (resume) {
        assert_retval(this->magic == MAGIC, -1);
        assert_retval(this->shmid == shmid, -1);
        assert_retval(this->shm_size == shm_size, -1);

        // reset pid here as it must have been changed
        this->pid = getpid();
    } else {
        this->pid = getpid();
        this->shmid = shmid;
        this->shm_size = shm_size;
        // TODO: may do some alignment job here
        this->used_size = sizeof(channel_mgr);

        lock.init();
        descriptor_count = 0;
        memset(descriptors, 0x00, sizeof(descriptors));

        // start a full memory barrier to make sure magic is set at the last step
        __sync_synchronize();

        // we set magic at the last, to ensure all fields are initialized
        this->magic = MAGIC;
    }

    return 0;
}

void channel_mgr::report() const {
    sk_info("===================================");
    for (int i = 0; i < descriptor_count; ++i) {
        const channel_descriptor& desc = descriptors[i];
        const channel *rc = get_read_channel(i);
        const channel *wc = get_write_channel(i);
        sk_info("channel<%x>, r<%lu>, w<%lu>, closed<%s>.",
                desc.owner, rc ? rc->message_count() : 0,
                wc ? wc->message_count() : 0, desc.closed ? "true" : "false");
    }
    sk_info("===================================");
}

int channel_mgr::register_channel(int busid, pid_t pid, size_t node_size, size_t node_count, int& fd) {
    if (magic != MAGIC) {
        sk_error("channel mgr has not been initialized.");
        return -EINVAL;
    }

    if ((node_size & (node_size - 1)) != 0) {
        sk_error("node size %lu must be 2 ^ N.", node_size);
        return -EINVAL;
    }

    fd = -1;
    channel_descriptor *desc = nullptr;

    do {
        lock_guard<spin_lock> guard(lock);
        for (int i = 0; i < descriptor_count; ++i) {
            channel_descriptor& desc = descriptors[i];
            check_continue(desc.owner == busid);

            if (!desc.closed) {
                sk_info("channel already exists, bus<%x>.", busid);
                fd = i;
                desc.pid = pid;
                return 0;
            }

            sk_info("channel<%x> closed, reopen it.", busid);

            channel *rc = sk::byte_offset<channel>(this, desc.r_offset);
            assert_retval(rc->magic == MAGIC, -1);
            rc->clear();

            channel *wc = sk::byte_offset<channel>(this, desc.w_offset);
            assert_retval(wc->magic == MAGIC, -1);
            wc->clear();

            sk_assert(rc->node_size == wc->node_size);
            sk_assert(rc->node_count == wc->node_count);

            if (rc->node_size != node_size || rc->node_count != node_count)
                sk_warn("configuration change<%d:%d -> %d:%d> is not supported.",
                        rc->node_size, rc->node_count, node_size, node_count);

            int ret = notify_channel_change(this->pid, i);
            if (ret != 0) {
                sk_error("cannot send signal: %s", strerror(errno));
                return ret;
            }

            desc.closed = 0;
            desc.pid = pid;
            fd = i;

            return 0;
        }

        size_t channel_size = channel::calc_space(node_size, node_count);
        assert_retval(channel_size > 0, -1);

        size_t left_size = 0;
        if (shm_size > used_size)
            left_size = shm_size - used_size;

        // mulitple 2 here because there are two channels, one for read & another for write
        if (left_size < channel_size * 2) {
            sk_error("left size %lu is not enough, required: %lu.", left_size, channel_size * 2);
            return -ENOMEM;
        }

        fd = descriptor_count++;
        desc = &descriptors[fd];
        desc->owner = busid;
        desc->closed = 0;
        desc->pid = pid;
        desc->r_offset = used_size;
        used_size += channel_size;
        desc->w_offset = used_size;
        used_size += channel_size;

        int ret = 0;

        channel *rc = sk::byte_offset<channel>(this, desc->r_offset);
        ret = rc->init(node_size, node_count);
        if (ret != 0) {
            sk_error("failed to init read channel, bus id<%x>, ret<%d>.", busid, ret);
            return ret;
        }

        channel *wc = sk::byte_offset<channel>(this, desc->w_offset);
        ret = wc->init(node_size, node_count);
        if (ret != 0) {
            sk_error("failed to init write channel, bus id<%x>, ret<%d>.", busid, ret);
            return ret;
        }

        ret = notify_channel_change(this->pid, fd);
        if (ret != 0) {
            sk_error("cannot send signal: %s", strerror(errno));
            return ret;
        }

        sk_info("new channel, fd<%d>, owner<%x>, read offset<%lu>, write offset<%lu>.",
                fd, desc->owner, desc->r_offset, desc->w_offset);
    } while (0);

    return 0;
}

void channel_mgr::deregister_channel(int busid) {
    lock_guard<spin_lock> guard(lock);
    for (int i = 0; i < descriptor_count; ++i) {
        channel_descriptor& desc = descriptors[i];
        check_continue(desc.owner == busid);

        if (desc.closed) {
            sk_warn("channel<%x> has already been closed.", desc.owner);
            return;
        }

        desc.closed = 1;
        desc.pid = 0;

        int ret = notify_channel_change(this->pid, i);
        if (ret != 0) sk_error("cannot send signal: %s", strerror(errno));

        sk_info("channel<%x> gets closed.", desc.owner);
        return;
    }
}

channel *channel_mgr::get_read_channel(int fd) {
    assert_retval(fd >= 0 && fd < descriptor_count, nullptr);

    channel_descriptor& desc = descriptors[fd];
    if (desc.closed) {
        sk_error("channel<%x> has been closed.", desc.owner);
        return nullptr;
    }

    channel *rc = sk::byte_offset<channel>(this, desc.r_offset);
    assert_retval(rc->magic == MAGIC, nullptr);

    return rc;
}

channel *channel_mgr::get_write_channel(int fd) {
    assert_retval(fd >= 0 && fd < descriptor_count, nullptr);

    channel_descriptor& desc = descriptors[fd];
    if (desc.closed) {
        sk_error("channel<%x> has been closed.", desc.owner);
        return nullptr;
    }

    channel *wc = sk::byte_offset<channel>(this, desc.w_offset);
    assert_retval(wc->magic == MAGIC, nullptr);

    return wc;
}

const channel *channel_mgr::get_read_channel(int fd) const {
    assert_retval(fd >= 0 && fd < descriptor_count, nullptr);

    const channel_descriptor& desc = descriptors[fd];
    if (desc.closed) {
        sk_error("channel<%x> has been closed.", desc.owner);
        return nullptr;
    }

    const channel *rc = sk::byte_offset<channel>(this, desc.r_offset);
    assert_retval(rc->magic == MAGIC, nullptr);

    return rc;
}

const channel *channel_mgr::get_write_channel(int fd) const {
    assert_retval(fd >= 0 && fd < descriptor_count, nullptr);

    const channel_descriptor& desc = descriptors[fd];
    if (desc.closed) {
        sk_error("channel<%x> has been closed.", desc.owner);
        return nullptr;
    }

    const channel *wc = sk::byte_offset<channel>(this, desc.w_offset);
    assert_retval(wc->magic == MAGIC, nullptr);

    return wc;
}

int channel_mgr::get_owner_busid(int fd) const {
    assert_retval(fd >= 0 && fd < descriptor_count, -1);
    return descriptors[fd].owner;
}

channel *channel_mgr::find_read_channel(int busid, int& fd) {
    for (int i = 0; i < descriptor_count; ++i) {
        if (descriptors[i].owner == busid) {
            fd = i;
            return get_read_channel(i);
        }
    }

    return nullptr;
}

NS_END(detail)
NS_END(sk)
