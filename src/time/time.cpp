#include <sys/time.h>
#include "time.h"
#include "shm/shm_ptr.h"
#include "time/shm_timer.h"
#include "time/heap_timer.h"

#define TICK_PER_SEC 50 // 50 tick per second (1 tick == 20ms)

struct time_mgr {
    bool in_shm;      // true if this instance is allocated in shared memory
    int time_offset;  // offset of real time and process time
    int start_sec;    // at which second the process started
    int time_sec;     // current process time, in second
    u64 time_usec;    // current process time, in micro second
    u64 current_tick; // current tick

    static u64 __ms2tick(u64 ms) {
        return (ms / 1000) * TICK_PER_SEC;
    }

    u64 __outdated_tick_count(const struct timeval *tv) const {
        u64 real_tick = __ms2tick((tv->tv_sec - start_sec) * 1000 + (tv->tv_usec / 1000));
        if (real_tick >= current_tick)
            return real_tick - current_tick;

        sk_info("leap second! real<%lu>, current<%lu>.", real_tick, current_tick);
        return 0;
    }

    void update_time() {
        struct timeval tv;
        gettimeofday(&tv, nullptr);

        u64 tick_count = __outdated_tick_count(&tv);
        while (tick_count > 0) {
            for (u64 i = 0; i < tick_count; ++i) {
                if (sk::time::shm_timer_enabled())
                    sk::time::run_shm_timer();

                if (sk::time::heap_timer_enabled())
                    sk::time::run_heap_timer();

                ++current_tick;
            }

            gettimeofday(&tv, nullptr);
            tick_count = __outdated_tick_count(&tv);
        }

        time_sec = tv.tv_sec + time_offset;
        time_usec = static_cast<u64>(time_sec) * 1000000 + tv.tv_usec;
    }

    time_mgr(int time_offset, bool in_shm) {
        struct timeval tv;
        gettimeofday(&tv, nullptr);

        this->in_shm       = in_shm;
        this->time_offset  = time_offset;
        this->start_sec    = tv.tv_sec;
        this->current_tick = __ms2tick(tv.tv_usec / 1000);
        this->time_sec     = tv.tv_sec + this->time_offset;
        this->time_usec    = this->time_sec * 1000000 + tv.tv_usec;
    }
};
static_assert(std::is_standard_layout<time_mgr>::value, "time_mgr must be a POD type");
static time_mgr *mgr = nullptr;

NS_BEGIN(sk)
NS_BEGIN(time)

int now() {
    return mgr->time_sec;
}

bool time_enabled(bool *in_shm) {
    if (in_shm)
        *in_shm = mgr && mgr->in_shm;

    return mgr != nullptr;
}

u64 sec2tick(u32 sec) {
    return sec * TICK_PER_SEC;
}

u64 current_tick() {
    return mgr->current_tick;
}

int init_time(int shm_type, int time_offset) {
    shm_ptr<time_mgr> ptr = shm_get_singleton<time_mgr>(shm_type, time_offset, true);
    assert_retval(ptr, -ENOMEM);

    mgr = ptr.get();
    return 0;
}

int init_time(int time_offset) {
    mgr = new time_mgr(time_offset, false);
    assert_retval(mgr, -ENOMEM);

    return 0;
}

void update_time() {
    mgr->update_time();
}

NS_END(time)
NS_END(sk)
