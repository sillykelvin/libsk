#include <utility>
#include <sys/time.h>
#include "timer.h"
#include "shm/shm_mgr.h"
#include "shm/shm_ptr.h"
#include "shm/detail/offset_ptr.h"
#include "utility/callback_dispatcher.h"

#define REPEAT_FOREVER -1

#define TVN_BITS 6
#define TVR_BITS 8
#define TVN_SIZE (1 << TVN_BITS)
#define TVR_SIZE (1 << TVR_BITS)
#define TVN_MASK (TVN_SIZE - 1)
#define TVR_MASK (TVR_SIZE - 1)

#define TV1_MAX TVR_SIZE
#define TV2_MAX (1 << (TVR_BITS + 1 * TVN_BITS))
#define TV3_MAX (1 << (TVR_BITS + 2 * TVN_BITS))
#define TV4_MAX (1 << (TVR_BITS + 3 * TVN_BITS))

#define TV1_IDX(tick) ((tick) & TVR_MASK)
#define TV2_IDX(tick) (((tick) >> (TVR_BITS + 0 * TVN_BITS)) & TVN_MASK)
#define TV3_IDX(tick) (((tick) >> (TVR_BITS + 1 * TVN_BITS)) & TVN_MASK)
#define TV4_IDX(tick) (((tick) >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK)
#define TV5_IDX(tick) (((tick) >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK)

typedef sk::callback_dispatcher<0, int, void(u64, void *, size_t)> timeout_dispatcher;

struct timer;
typedef sk::detail::offset_ptr<timer> timer_ptr;
static inline void timer_list_init(timer_ptr list);
static inline bool timer_list_empty(timer_ptr list);
static inline void timer_list_remove(timer_ptr node);
static inline void timer_list_append(timer_ptr list, timer_ptr node);

struct timer {
    u64 timer_mid;   // mid to this timer
    u64 expires;     // tick to next timeout
    bool running;    // is in timeout callback or not
    u32 intreval;    // timeout interval, in tick
    int repeats;     // repeat count, forever if it's -1
    int timer_type;  // callback id
    timer_ptr prev;  // prev timer
    timer_ptr next;  // next timer
    size_t cb_len;   // callback data length
    char cb_data[0]; // callback data

    inline timer_ptr ptr() {
        offset_t offset = sk::shm_mgr::get()->ptr2offset(this);
        return timer_ptr(offset);
    }
};

struct timer_vec {
    timer vec[TVN_SIZE]; // dummy heads

    void init() {
        memset(vec, 0x00, sizeof(vec));

        for (int i = 0; i < (int) array_len(vec); ++i) {
            timer_ptr ptr = vec[i].ptr();
            timer_list_init(ptr);
        }
    }
};

struct timer_vec_root {
    timer vec[TVR_SIZE]; // dummy heads

    void init() {
        memset(vec, 0x00, sizeof(vec));

        for (int i = 0; i < (int) array_len(vec); ++i) {
            timer_ptr ptr = vec[i].ptr();
            timer_list_init(ptr);
        }
    }
};

struct timer_mgr {
    int time_offset;  // offset of real time and process time
    u64 current_tick; // current tick
    int start_sec;    // at which second the process started

    int time_sec;     // current process time, in second
    u64 time_usec;    // current process time, in micro second

    timer_vec_root tv1;
    timer_vec tv2;
    timer_vec tv3;
    timer_vec tv4;
    timer_vec tv5;

    timer_mgr(int time_offset) {
        this->time_offset = time_offset;

        struct timeval tv;
        gettimeofday(&tv, NULL);

        this->start_sec = tv.tv_sec;
        this->time_sec = tv.tv_sec + this->time_offset;
        this->time_usec = static_cast<u64>(this->time_sec) * 1000000 + tv.tv_usec;
        this->current_tick = sk::time::ms2tick(tv.tv_usec / 1000);

        tv1.init();
        tv2.init();
        tv3.init();
        tv4.init();
        tv5.init();
    }
};
static_assert(std::is_standard_layout<timer_mgr>::value, "timer_mgr must be a POD type");

static timer_mgr *mgr = NULL;

static inline void timer_list_init(timer_ptr list) {
    timer *t = list.get();
    t->prev = list;
    t->next = list;
}

static inline bool timer_list_empty(timer_ptr list) {
    timer *t = list.get();
    bool empty = (t->next == list);
    if (empty)
        sk_assert(t->prev == list);

    return empty;
}

static inline void timer_list_remove(timer_ptr node) {
    timer *n = node.get();
    timer *prev = n->prev.get();
    timer *next = n->next.get();

    prev->next = n->next;
    next->prev = n->prev;
    n->prev.set_null();
    n->next.set_null();
}

static inline void timer_list_append(timer_ptr list, timer_ptr node) {
    timer *l = list.get();
    timer *n = node.get();
    timer *prev = l->prev.get();

    assert_retnone(!n->prev);
    assert_retnone(!n->next);

    n->prev = l->prev;
    n->next = list;
    prev->next = node;
    l->prev = node;
}

static timer_ptr search_list(u64 expires) {
    sk_assert(expires >= mgr->current_tick);

    u64 delta = expires - mgr->current_tick;

    if (delta < TV1_MAX)
        return mgr->tv1.vec[TV1_IDX(expires)].ptr();

    if (delta < TV2_MAX)
        return mgr->tv2.vec[TV2_IDX(expires)].ptr();

    if (delta < TV3_MAX)
        return mgr->tv3.vec[TV3_IDX(expires)].ptr();

    if (delta < TV4_MAX)
        return mgr->tv4.vec[TV4_IDX(expires)].ptr();

    // if the timeout interval is larger than 0xFFFFFFFF
    // on 64-bit machine, then we use the maximum interval
    if (delta > 0xffffffffUL) {
        delta = 0xffffffffUL;
        expires = delta + mgr->current_tick;
    }

    return mgr->tv5.vec[TV5_IDX(expires)].ptr();
}

static u64 real_tick() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return sk::time::ms2tick((tv.tv_sec - mgr->start_sec) * 1000 + (tv.tv_usec / 1000));
}

static void set_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    mgr->time_sec = tv.tv_sec + mgr->time_offset;
    mgr->time_usec = static_cast<u64>(mgr->time_sec) * 1000000 + tv.tv_usec;
}

static void cascade(timer_ptr list) {
    timer *l = list.get();
    while(!timer_list_empty(list)) {
        timer_ptr ptr = l->next;
        timer *t = ptr.get();

        timer_ptr new_list = search_list(t->expires);
        timer_list_remove(ptr);
        timer_list_append(new_list, ptr);
    }
}

static void do_run_timer() {
    int tv1_idx = TV1_IDX(mgr->current_tick);

    do {
        if (tv1_idx != 0) break;

        int tv2_idx = TV2_IDX(mgr->current_tick);
        cascade(mgr->tv2.vec[tv2_idx].ptr());
        if (tv2_idx != 0) break;

        int tv3_idx = TV3_IDX(mgr->current_tick);
        cascade(mgr->tv3.vec[tv3_idx].ptr());
        if (tv3_idx != 0) break;

        int tv4_idx = TV4_IDX(mgr->current_tick);
        cascade(mgr->tv4.vec[tv4_idx].ptr());
        if (tv4_idx != 0) break;

        int tv5_idx = TV5_IDX(mgr->current_tick);
        cascade(mgr->tv5.vec[tv5_idx].ptr());
    } while (0);

    timer_ptr list = mgr->tv1.vec[tv1_idx].ptr();
    timer *l = list.get();

    while (!timer_list_empty(list)) {
        timer_ptr ptr = l->next;
        timer *t = ptr.get();

        // the timer is "running" before calling the registered callback,
        // then it must be that the process crashed when calling the callback
        // last time, so, just remove the "ill" timer
        if (t->running) {
            sk_error("timer callback crashed at last time, type<%d>.", t->timer_type);
            if (t->repeats <= 0 && t->repeats != REPEAT_FOREVER) {
                timer_list_remove(ptr);
                sk::shm_free(sk::shm_ptr<timer>(t->timer_mid));
                continue;
            }
        }

        t->running = true;

        if (t->repeats > 0 || t->repeats == REPEAT_FOREVER) {
            if (t->repeats > 0)
                --t->repeats;

            void *cb = NULL;
            if (t->cb_len > 0) cb = t->cb_data;

            timeout_dispatcher::get()->dispatch(t->timer_type, t->timer_mid, cb, t->cb_len);
        }

        t->running = false;

        if (t->repeats > 0 || t->repeats == REPEAT_FOREVER) {
            t->expires = mgr->current_tick + t->intreval;
            timer_ptr new_list = search_list(t->expires);
            timer_list_remove(ptr);
            timer_list_append(new_list, ptr);
            continue;
        }

        sk_info("timer<%lu> finished, type<%d>.", t->timer_mid, t->timer_type);
        timer_list_remove(ptr);
        sk::shm_free(sk::shm_ptr<timer>(t->timer_mid));
    }
}

namespace sk {
namespace time {

int register_timeout_callback(int timer_type, TIMEOUT_CALLBACK fn_callback) {
    if (timeout_dispatcher::get()->has_handler(timer_type))
        return -EEXIST;

    timeout_dispatcher::get()->register_handler(timer_type, fn_callback);
    return 0;
}

u64 add_timer(u32 first_interval_sec, u32 repeat_interval_sec,
              int repeat_count, int timer_type,
              const void *cb_data, size_t cb_len) {
    assert_retval(repeat_count == REPEAT_FOREVER || repeat_count >= 1, -EINVAL);

    if (!timeout_dispatcher::get()->has_handler(timer_type)) {
        sk_assert(0);
        return MID_NULL;
    }

    if (repeat_interval_sec == 0 && repeat_count == REPEAT_FOREVER) {
        assert_retval(0, MID_NULL);
    }

    u32 first_interval_tick = sec2tick(first_interval_sec);
    u32 repeat_interval_tick = sec2tick(repeat_interval_sec);

    size_t data_len = 0;
    if (cb_data && cb_len > 0)
        data_len = cb_len;

    size_t mem_len = sizeof(timer) + data_len;
    shm_ptr<timer> ptr = sk::shm_malloc(mem_len);
    assert_retval(ptr, MID_NULL);

    timer *t = ptr.get();
    t->timer_mid = ptr.mid;
    t->expires = mgr->current_tick + first_interval_tick;
    t->running = false;
    t->intreval = repeat_interval_tick;
    t->repeats = repeat_count;
    t->timer_type = timer_type;
    t->prev.set_null();
    t->next.set_null();

    t->cb_len = data_len;
    if (data_len > 0)
        memcpy(t->cb_data, cb_data, cb_len);

    timer_ptr list = search_list(t->expires);
    timer_list_append(list, t->ptr());

    return ptr.mid;
}

u64 add_timer(u32 interval_sec, int repeat_count,
              int timer_type, const void *cb_data, size_t cb_len) {
    return add_timer(interval_sec, interval_sec, repeat_count, timer_type, cb_data, cb_len);
}

u64 add_forever_timer(u32 first_interval_sec, u32 repeat_interval_sec,
                      int timer_type, const void *cb_data, size_t cb_len) {
    return add_timer(first_interval_sec, repeat_interval_sec,
                     REPEAT_FOREVER, timer_type, cb_data, cb_len);
}

u64 add_forever_timer(u32 interval_sec, int timer_type,
                      const void *cb_data, size_t cb_len) {
    return add_forever_timer(interval_sec, interval_sec, timer_type, cb_data, cb_len);
}

u64 add_single_timer(u32 interval_sec, int timer_type,
                     const void *cb_data, size_t cb_len) {
    return add_timer(interval_sec, 0, 1, timer_type, cb_data, cb_len);
}

void remove_timer(u64 timer_mid) {
    if (timer_mid == MID_NULL) return;

    shm_ptr<timer> ptr(timer_mid);
    timer *t = ptr.get();
    assert_retnone(t);

    if (t->running) {
        t->repeats = 0;
        return;
    }

    timer_list_remove(t->ptr());
    shm_free(ptr);
}

int now() {
    return mgr->time_sec;
}

u64 current_tick() {
    return mgr->current_tick;
}

u64 current_msec() {
    return mgr->time_usec / 1000;
}

u64 current_usec() {
    return mgr->time_usec;
}

bool timer_enabled() {
    return mgr != NULL;
}

int init_timer(int shm_type, int time_offset) {
    shm_ptr<timer_mgr> ptr = shm_get_singleton<timer_mgr>(shm_type, time_offset);
    assert_retval(ptr, -ENOMEM);

    mgr = ptr.get();
    return 0;
}

void run_timer() {
    u64 sys_tick = real_tick();

    /*
     * there is a bug here:
     *     during a leap second, sys_tick might be less than mgr->current_tick,
     * the assertion will fail, and tick_count will be a very very huge number,
     * then the followed while(...) loop gets into an "almost" infinite loop...
     */
    // sk_assert(sys_tick >= mgr->current_tick);
    // u64 tick_count = sys_tick - mgr->current_tick;

    /*
     * bug fix:
     *     instead of assertion, check sys_tick and mgr->current_tick, only run
     * the loop if sys_tick > mgr->current_tick
     */
    u64 tick_count = 0;
    if (sys_tick < mgr->current_tick)
        sk_info("leap second! sys_tick<%lu>, current_tick<%lu>.",
                sys_tick, mgr->current_tick);
    else
        tick_count = sys_tick - mgr->current_tick;

    while (tick_count > 0) {
        for (u64 i = 0; i < tick_count; ++i) {
            do_run_timer();
            ++mgr->current_tick;
        }

        // tick_count = real_tick() - mgr->current_tick;

        tick_count = 0;
        sys_tick = real_tick();
        if (sys_tick < mgr->current_tick)
            sk_info("leap second! sys_tick<%lu>, current_tick<%lu>.",
                    sys_tick, mgr->current_tick);
        else
            tick_count = sys_tick - mgr->current_tick;
    }

    set_time();
}

} // namespace time
} // namespace sk
