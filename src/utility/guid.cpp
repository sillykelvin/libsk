#include <map>
#include "guid.h"
#include "bus/bus.h"
#include "time/timer.h"
#include "utility/assert_helper.h"

static const int      INST_BIT_COUNT = 3;
static const int      FUNC_BIT_COUNT = 5;
static const int      ZONE_BIT_COUNT = 10;
static const int    SERIAL_BIT_COUNT = 16;
static const int TIMESTAMP_BIT_COUNT = 30;

struct GUID {
    u64 inst:           INST_BIT_COUNT;
    u64 func:           FUNC_BIT_COUNT;
    u64 zone:           ZONE_BIT_COUNT;
    u64 serial:       SERIAL_BIT_COUNT;
    u64 timestamp: TIMESTAMP_BIT_COUNT;
};
static_assert(sizeof(GUID) == sizeof(u64), "GUID size invalid");

struct GUID_VALUE {
    time_t last_sec;
    u64 last_serial;
};

struct GUID_KEY {
    int inst;
    int func;
    int zone;

    bool operator<(const GUID_KEY& that) const {
        if (this->zone != that.zone)
            return this->zone < that.zone;

        if (this->func != that.func)
            return this->func < that.func;

        return this->inst < that.inst;
    }

    GUID_KEY(int inst, int func, int zone)
        : inst(inst), func(func), zone(zone) {}
};

static std::map<GUID_KEY, GUID_VALUE> key2guid;

namespace sk {
namespace guid {

int create(u64& guid) {
    guid = 0;

    int inst = sk::bus::inst_id();
    assert_retval(inst > 0, -EAGAIN);
    assert_retval(inst < (1 << INST_BIT_COUNT), -E2BIG);

    int func = sk::bus::func_id();
    assert_retval(func > 0, -EAGAIN);
    assert_retval(func < (1 << FUNC_BIT_COUNT), -E2BIG);

    int zone = sk::bus::zone_id();
    assert_retval(zone >= 0, -EAGAIN);
    assert_retval(zone < (1 << ZONE_BIT_COUNT), -E2BIG);

    GUID_KEY key(inst, func, zone);
    auto it = key2guid.find(key);
    if (it == key2guid.end()) {
        GUID_VALUE value = {0};
        auto ret = key2guid.insert(std::pair<GUID_KEY, GUID_VALUE>(key, value));
        if (!ret.second) {
            sk_assert(0);
            return -ENOMEM;
        }

        it = ret.first;
    }

    assert_retval(it != key2guid.end(), -EINVAL);

    GUID_VALUE& value = it->second;

    time_t t = time::now();
    if (value.last_sec != t) {
        value.last_sec = t;
        value.last_serial = 0;
    }

    // reached the max allocation count per second
    if (value.last_serial >= (1 << SERIAL_BIT_COUNT)) {
        sk_assert(0);
        return -EBUSY;
    }

    GUID *g = cast_ptr(GUID, &guid);
    g->inst = inst;
    g->func = func;
    g->zone = zone;
    g->serial = value.last_serial;
    g->timestamp = value.last_sec & ((1 << TIMESTAMP_BIT_COUNT) - 1);

    ++value.last_serial;

    return 0;
}

} // namespace guid
} // namespace sk
