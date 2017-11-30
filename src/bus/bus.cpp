#include <bus/bus.h>
#include <log/log.h>
#include <bus/detail/channel.h>
#include <utility/assert_helper.h>
#include <shm/detail/shm_object.h>
#include <bus/detail/channel_mgr.h>

NS_BEGIN(sk)
NS_BEGIN(bus)

static int fd = -1;
static int busid = -1;
static detail::channel_mgr *mgr = nullptr;

union busid_format {
    int busid;
    struct {
        u8 area_id;
        u8 zone_id;
        u8 func_id;
        u8 inst_id;
    };
};
static_assert(sizeof(busid_format) == sizeof(int), "invalid type: busid_format");

static u64 time_ns() {
    struct timespec t;
    int ret = clock_gettime(CLOCK_MONOTONIC, &t);
    if (unlikely(ret != 0)) return 0;
    return static_cast<u64>(t.tv_sec) * 1000000000 + static_cast<u64>(t.tv_nsec);
}

int from_string(const char *str, int *area_id, int *zone_id, int *func_id, int *inst_id) {
    assert_retval(str, -1);

    int a_id = 0, z_id = 0, f_id = 0, i_id = 0;
    int ret = sscanf(str, "%d.%d.%d.%d", &a_id, &z_id, &f_id, &i_id);
    if (ret != 4) return -1;

    if (area_id) *area_id = a_id;
    if (zone_id) *zone_id = z_id;
    if (func_id) *func_id = f_id;
    if (inst_id) *inst_id = i_id;

    busid_format f;
    f.area_id = a_id;
    f.zone_id = z_id;
    f.func_id = f_id;
    f.inst_id = i_id;

    return f.busid;
}

int from_subid(int area_id, int zone_id, int func_id, int inst_id) {
    busid_format f;
    f.area_id = area_id;
    f.zone_id = zone_id;
    f.func_id = func_id;
    f.inst_id = inst_id;

    return f.busid;
}

std::string to_string(int bus_id) {
    busid_format f;
    f.busid = bus_id;

    char buf[128] = {0};
    snprintf(buf, sizeof(buf) - 1, "%d.%d.%d.%d", f.area_id, f.zone_id, f.func_id, f.inst_id);

    return buf;
}

int bus_id() {
    return busid;
}

int area_id() {
    busid_format f;
    f.busid = busid;

    return f.area_id;
}

int zone_id() {
    busid_format f;
    f.busid = busid;

    return f.zone_id;
}

int func_id() {
    busid_format f;
    f.busid = busid;

    return f.func_id;
}

int inst_id() {
    busid_format f;
    f.busid = busid;

    return f.inst_id;
}

int area_id(int bus_id) {
    busid_format f;
    f.busid = bus_id;

    return f.area_id;
}

int zone_id(int bus_id) {
    busid_format f;
    f.busid = bus_id;

    return f.zone_id;
}

int func_id(int bus_id) {
    busid_format f;
    f.busid = bus_id;

    return f.func_id;
}

int inst_id(int bus_id) {
    busid_format f;
    f.busid = bus_id;

    return f.inst_id;
}

int register_bus(const char *shm_path, int busid, size_t node_size, size_t node_count) {
    if (fd != -1) {
        sk_error("bus<%x> already registered.", busid);
        return -EINVAL;
    }

    int ret = 0;
    size_t shm_size = 0;

    int shmfd = detail::shm_object_attach(shm_path, &shm_size);
    if (shmfd == -1) return shmfd;

    void *addr = detail::shm_object_map(shmfd, &shm_size, 0);
    if (!addr) {
        close(shmfd);
        return -1;
    }

    close(shmfd);
    mgr = cast_ptr(detail::channel_mgr, addr);

    pid_t pid = getpid();
    ret = mgr->register_channel(busid, pid, node_size, node_count, fd);
    if (ret != 0) return ret;

    sk::bus::busid = busid;

    sk_info("bus registered, bus id<%x>, fd<%d>.", busid, fd);
    return 0;
}

void deregister_bus() {
    if (fd == -1) sk_warn("bus<%x> seems to be deregistered.", busid);

    mgr->deregister_channel(busid);
    fd = -1;

    // we do NOT reset busid here, in case functions like
    // sk::bus::area_id() might get called after deregistration
    //sk::bus::busid = -1;

    sk_info("bus deregistered, bus id<%x>.", busid);
}

int send(int dst_busid, const void *data, size_t length) {
    assert_retval(fd != -1, -1);
    assert_retval(mgr, -1);
    assert_retval(data, -1);

    if (unlikely(dst_busid <= 0)) {
        sk_error("invalid bus id<%x>.", dst_busid);
        return -EINVAL;
    }

    detail::channel *wc = mgr->get_write_channel(fd);
    assert_retval(wc, -1);

    int src_busid = mgr->get_owner_busid(fd);
    assert_retval(src_busid > 0, -1);

    u64 now = time_ns();
    int retry = 3;
    while (retry-- > 0) {
        int ret = wc->push(src_busid, dst_busid, now, data, length);
        if (likely(ret == 0)) break;

        sk_error("failed to write data to bus, ret<%d>, retry<%d>.", ret, retry);
    }

    sigval value;
    memset(&value, 0x00, sizeof(value));
    value.sival_int = fd;
    int ret = sigqueue(mgr->pid, BUS_OUTGOING_SIGNO, value);
    if (ret != 0) sk_warn("cannot send signal: %s", strerror(errno));

    return 0;
}

int recv(int& src_busid, void *data, size_t& length) {
    assert_retval(fd != -1, -1);
    assert_retval(mgr, -1);
    assert_retval(data, -1);

    detail::channel *rc = mgr->get_read_channel(fd);
    assert_retval(rc, -1);

    int dst_busid = 0;
    u64 send_time = 0;
    int count = rc->pop(data, length, &src_busid, &dst_busid, &send_time);
    // TODO: do some performance counter with send_time here
    if (count == 0 || count == 1)
        return count;

    sk_error("failed to read data from bus, error<%d>.", count);
    return count;
}

NS_END(bus)
NS_END(sk)
