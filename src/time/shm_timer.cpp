#include "shm_timer.h"
#include "time/time.h"
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

struct shm_timer;
typedef sk::detail::offset_ptr<shm_timer> shm_timer_ptr;
static inline void timer_list_init(shm_timer_ptr list);
static inline bool timer_list_empty(shm_timer_ptr list);
static inline void timer_list_remove(shm_timer_ptr node);
static inline void timer_list_append(shm_timer_ptr list, shm_timer_ptr node);

struct shm_timer {
    u64 timer_mid;      // mid to this shm timer
    u64 expires;        // tick to next timeout
    u64 intreval;       // timeout interval, in tick
    bool running;       // is in timeout callback or not
    int repeats;        // repeat count, forever if it's -1
    int timer_type;     // callback id
    shm_timer_ptr prev; // prev timer
    shm_timer_ptr next; // next timer
    size_t cb_len;      // callback data length
    char cb_data[0];    // callback data

    inline shm_timer_ptr ptr() {
        offset_t offset = sk::shm_mgr::get()->ptr2offset(this);
        return shm_timer_ptr(offset);
    }
};

struct timer_vec {
    shm_timer vec[TVN_SIZE]; // dummy heads

    void init() {
        memset(vec, 0x00, sizeof(vec));

        for (int i = 0; i < (int) array_len(vec); ++i) {
            shm_timer_ptr ptr = vec[i].ptr();
            timer_list_init(ptr);
        }
    }
};

struct timer_vec_root {
    shm_timer vec[TVR_SIZE]; // dummy heads

    void init() {
        memset(vec, 0x00, sizeof(vec));

        for (int i = 0; i < (int) array_len(vec); ++i) {
            shm_timer_ptr ptr = vec[i].ptr();
            timer_list_init(ptr);
        }
    }
};

struct shm_timer_mgr {
    timer_vec_root tv1;
    timer_vec tv2;
    timer_vec tv3;
    timer_vec tv4;
    timer_vec tv5;

    shm_timer_mgr() {
        tv1.init();
        tv2.init();
        tv3.init();
        tv4.init();
        tv5.init();
    }

    shm_timer_ptr __search_list(u64 expires) {
        u64 current_tick = sk::time::current_tick();
        sk_assert(expires >= current_tick);

        u64 delta = expires - current_tick;

        if (delta < TV1_MAX)
            return tv1.vec[TV1_IDX(expires)].ptr();

        if (delta < TV2_MAX)
            return tv2.vec[TV2_IDX(expires)].ptr();

        if (delta < TV3_MAX)
            return tv3.vec[TV3_IDX(expires)].ptr();

        if (delta < TV4_MAX)
            return tv4.vec[TV4_IDX(expires)].ptr();

        // if the timeout interval is larger than 0xFFFFFFFF
        // on 64-bit machine, then we use the maximum interval
        if (delta > 0xffffffffUL) {
            delta = 0xffffffffUL;
            expires = delta + current_tick;
        }

        return tv5.vec[TV5_IDX(expires)].ptr();
    }

    void __cascade(shm_timer_ptr list) {
        shm_timer *l = list.get();
        while(!timer_list_empty(list)) {
            shm_timer_ptr ptr = l->next;
            shm_timer *t = ptr.get();

            shm_timer_ptr new_list = __search_list(t->expires);
            timer_list_remove(ptr);
            timer_list_append(new_list, ptr);
        }
    }

    void add_timer(shm_timer *timer) {
        shm_timer_ptr list = __search_list(timer->expires);
        timer_list_append(list, timer->ptr());
    }

    void run_timer() {
        u64 current_tick = sk::time::current_tick();
        int tv1_idx = TV1_IDX(current_tick);

        do {
            if (tv1_idx != 0) break;

            int tv2_idx = TV2_IDX(current_tick);
            __cascade(tv2.vec[tv2_idx].ptr());
            if (tv2_idx != 0) break;

            int tv3_idx = TV3_IDX(current_tick);
            __cascade(tv3.vec[tv3_idx].ptr());
            if (tv3_idx != 0) break;

            int tv4_idx = TV4_IDX(current_tick);
            __cascade(tv4.vec[tv4_idx].ptr());
            if (tv4_idx != 0) break;

            int tv5_idx = TV5_IDX(current_tick);
            __cascade(tv5.vec[tv5_idx].ptr());
        } while (0);

        shm_timer_ptr list = tv1.vec[tv1_idx].ptr();
        shm_timer *l = list.get();

        while (!timer_list_empty(list)) {
            shm_timer_ptr ptr = l->next;
            shm_timer *t = ptr.get();

            // the timer is "running" before calling the registered callback,
            // then it must be that the process crashed when calling the callback
            // last time, so, just remove the "ill" timer
            if (t->running) {
                sk_error("timer callback crashed at last time, type<%d>.", t->timer_type);
                if (t->repeats <= 0 && t->repeats != REPEAT_FOREVER) {
                    timer_list_remove(ptr);
                    sk::shm_free(sk::shm_ptr<shm_timer>(t->timer_mid));
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
                t->expires = current_tick + t->intreval;
                shm_timer_ptr new_list = __search_list(t->expires);
                timer_list_remove(ptr);
                timer_list_append(new_list, ptr);
                continue;
            }

            sk_info("timer<%lu> finished, type<%d>.", t->timer_mid, t->timer_type);
            timer_list_remove(ptr);
            sk::shm_free(sk::shm_ptr<shm_timer>(t->timer_mid));
        }
    }
};
static_assert(std::is_standard_layout<shm_timer_mgr>::value, "shm_timer_mgr must be a POD type");
static shm_timer_mgr *mgr = nullptr;

static inline void timer_list_init(shm_timer_ptr list) {
    shm_timer *t = list.get();
    t->prev = list;
    t->next = list;
}

static inline bool timer_list_empty(shm_timer_ptr list) {
    shm_timer *t = list.get();
    bool empty = (t->next == list);
    if (empty)
        sk_assert(t->prev == list);

    return empty;
}

static inline void timer_list_remove(shm_timer_ptr node) {
    shm_timer *n = node.get();
    shm_timer *prev = n->prev.get();
    shm_timer *next = n->next.get();

    prev->next = n->next;
    next->prev = n->prev;
    n->prev.set_null();
    n->next.set_null();
}

static inline void timer_list_append(shm_timer_ptr list, shm_timer_ptr node) {
    shm_timer *l = list.get();
    shm_timer *n = node.get();
    shm_timer *prev = l->prev.get();

    assert_retnone(!n->prev);
    assert_retnone(!n->next);

    n->prev = l->prev;
    n->next = list;
    prev->next = node;
    l->prev = node;
}

NS_BEGIN(sk)
NS_BEGIN(time)

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

    bool in_shm = false;
    bool ok = time_enabled(&in_shm);
    assert_retval(ok && in_shm, MID_NULL);

    if (!timeout_dispatcher::get()->has_handler(timer_type)) {
        sk_assert(0);
        return MID_NULL;
    }

    if (repeat_interval_sec == 0 && repeat_count == REPEAT_FOREVER) {
        assert_retval(0, MID_NULL);
    }

    size_t data_len = 0;
    if (cb_data && cb_len > 0)
        data_len = cb_len;

    size_t mem_len = sizeof(shm_timer) + data_len;
    shm_ptr<shm_timer> ptr = sk::shm_malloc(mem_len);
    assert_retval(ptr, MID_NULL);

    shm_timer *t = ptr.get();
    t->timer_mid = ptr.mid;
    t->expires = current_tick() + sec2tick(first_interval_sec);
    t->intreval = sec2tick(repeat_interval_sec);
    t->running = false;
    t->repeats = repeat_count;
    t->timer_type = timer_type;
    t->prev.set_null();
    t->next.set_null();

    t->cb_len = data_len;
    if (data_len > 0)
        memcpy(t->cb_data, cb_data, cb_len);

    mgr->add_timer(t);
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

    shm_ptr<shm_timer> ptr(timer_mid);
    shm_timer *t = ptr.get();
    assert_retnone(t);

    if (t->running) {
        t->repeats = 0;
        return;
    }

    timer_list_remove(t->ptr());
    shm_free(ptr);
}

bool shm_timer_enabled() {
    return mgr != nullptr;
}

void run_shm_timer() {
    mgr->run_timer();
}

int init_shm_timer(int shm_type) {
    shm_ptr<shm_timer_mgr> ptr = shm_get_singleton<shm_timer_mgr>(shm_type);
    assert_retval(ptr, -ENOMEM);

    mgr = ptr.get();
    return 0;
}

NS_END(time)
NS_END(sk)
