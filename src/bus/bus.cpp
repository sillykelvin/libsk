#include "bus.h"
#include "bus/detail/channel_mgr.h"
#include "bus/detail/channel.h"
#include "shm/detail/shm_segment.h"
#include "utility/assert_helper.h"

namespace sk {
namespace bus {

static detail::channel_mgr *mgr = NULL;

int register_bus(key_t bus_key, int busid, int& fd, size_t node_size, size_t node_count) {
    int ret = 0;

    detail::shm_segment seg;
    ret = seg.init(bus_key, 0, true);
    if (ret != 0) return ret;

    mgr = cast_ptr(detail::channel_mgr, seg.address());
    assert_retval(mgr, -1);

    // no matter it will succeed or not in the following logic, we
    // release the control here
    seg.release();

    ret = mgr->register_channel(busid, node_size, node_count, fd);
    if (ret != 0) return ret;

    info("bus registered, bus id<%x>, fd<%d>.", busid, fd);
    return 0;
}

int send(int fd, int dst_busid, const void *data, size_t length) {
    assert_retval(mgr, -1);
    assert_retval(data, -1);

    detail::channel *wc = mgr->get_write_channel(fd);
    assert_retval(wc, -1);

    int src_busid = mgr->get_owner_busid(fd);
    assert_retval(src_busid > 0, -1);

    int retry = 3;
    while (retry-- > 0) {
        int ret = wc->push(src_busid, dst_busid, data, length);
        if (ret == 0) break;

        error("failed to write data to bus, ret<%d>, retry<%d>.", ret, retry);
    }

    return 0;
}

int recv(int fd, int& src_busid, void *data, size_t& length) {
    assert_retval(mgr, -1);
    assert_retval(data, -1);

    detail::channel *rc = mgr->get_read_channel(fd);
    assert_retval(rc, -1);

    int dst_busid = 0;
    int count = rc->pop(data, length, &src_busid, &dst_busid);
    if (count == 0 || count == 1)
        return count;

    error("failed to read data from bus, error<%d>.", count);
    return count;
}

} // namespace bus
} // namespace sk
