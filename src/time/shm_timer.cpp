#include <shm/shm.h>
#include <sys/time.h>
#include <time/shm_timer.h>
#include <time/heap_timer.h>
#include <utility/time_helper.h>

#define TVN_BITS 6
#define TVR_BITS 8
#define TVN_SIZE (1 << TVN_BITS)
#define TVR_SIZE (1 << TVR_BITS)
#define TVN_MASK (TVN_SIZE - 1)
#define TVR_MASK (TVR_SIZE - 1)

#define TV1_MAX (1 << (TVR_BITS + 0 * TVN_BITS))
#define TV2_MAX (1 << (TVR_BITS + 1 * TVN_BITS))
#define TV3_MAX (1 << (TVR_BITS + 2 * TVN_BITS))
#define TV4_MAX (1 << (TVR_BITS + 3 * TVN_BITS))

#define TV1_IDX(tick) ((tick) & TVR_MASK)
#define TV2_IDX(tick) (((tick) >> (TVR_BITS + 0 * TVN_BITS)) & TVN_MASK)
#define TV3_IDX(tick) (((tick) >> (TVR_BITS + 1 * TVN_BITS)) & TVN_MASK)
#define TV4_IDX(tick) (((tick) >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK)
#define TV5_IDX(tick) (((tick) >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK)

NS_BEGIN(sk)
NS_BEGIN(time)

/*
 * timers with value not a multiple of 20ms will be
 * rounded to a multiple of 20ms, e.g. 17ms will be
 * 0ms, 105ms will be 100ms
 */
static const s64 TIMER_PRECISION_MS = 20; // 20 ms

class timer_helper {
public:
    static inline void timer_list_init(shm_timer_ptr list) {
        shm_timer *head = list.get();
        head->set_prev_ptr(list);
        head->set_next_ptr(list);
    }

    static inline bool timer_list_empty(shm_timer_ptr list) {
        shm_timer *head = list.get();
        bool empty = (head->next() == list);
        sk_assert(!empty || head->prev() == list);
        return empty;
    }

    static inline void timer_list_remove(shm_timer_ptr node) {
        shm_timer *n = node.get();
        shm_timer_ptr prev = n->prev();
        shm_timer_ptr next = n->next();

        prev->set_next_ptr(n->next());
        next->set_prev_ptr(n->prev());
        n->set_next_ptr(nullptr);
        n->set_prev_ptr(nullptr);
    }

    static inline void timer_list_append(shm_timer_ptr list, shm_timer_ptr node) {
        shm_timer *l = list.get();
        shm_timer *n = node.get();
        shm_timer_ptr prev = l->prev();
        assert_retnone(!n->prev() && !n->next());

        n->set_prev_ptr(l->prev());
        n->set_next_ptr(list);
        prev->set_next_ptr(node);
        l->set_prev_ptr(node);
    }
};

static inline s64 ms2tick(s64 ms) {
    return ms / TIMER_PRECISION_MS;
}

static inline s64 time2tick(const struct timeval *tv) {
    return ms2tick(tv->tv_sec * 1000 + tv->tv_usec / 1000);
}

template<size_t SIZE>
struct timer_vector {
    shm_timer_ptr vec[SIZE]; // dummy heads

    timer_vector() {
        for (size_t i = 0; i < SIZE; ++i) {
            shm_timer_ptr ptr = shm_timer::construct(0);
            assert_retnone(ptr);

            vec[i] = ptr;
            ptr->set_self_ptr(ptr);
            timer_helper::timer_list_init(ptr);
        }
    }

    ~timer_vector() {
        for (size_t i = 0; i < SIZE; ++i) {
            // TODO: shouldn't we clean the other timers on this list first??
            vec[i]->set_next_ptr(nullptr);
            vec[i]->set_prev_ptr(nullptr);
            shm_timer::destruct(vec[i]);
            vec[i] = nullptr;
        }
    }
};

class shm_timer_mgr {
public:
    explicit shm_timer_mgr(int time_offset_sec)
        : current_tick_(0) {
        time_offset_.tv_sec = time_offset_sec;
        time_offset_.tv_usec = 0;

        gettimeofday(&start_time_, nullptr);
        timeval_add(start_time_, time_offset_, &current_time_);
    }

    int init(uv_loop_t *loop) {
        heap_timer *timer = new heap_timer(loop,
                                           std::bind(&shm_timer_mgr::on_tick,
                                                     this, std::placeholders::_1));
        if (!timer) return -ENOMEM;

        // TODO: the better way here, is to add a tick timer, whose timeout value
        // is the duration between current time and the timeout time of the next
        // shm_timer, this will save a lot of empty ticks
        timer->start_forever(TIMER_PRECISION_MS, TIMER_PRECISION_MS);

        tick_timer_ = std::unique_ptr<heap_timer>(timer);
        return 0;
    }

    void stop() {
        if (tick_timer_) tick_timer_->stop();
    }

    s64 current_tick() const { return current_tick_; }
    int current_sec() const { return cast_int(current_time_.tv_sec); }

    void add_timer(shm_timer_ptr timer) {
        shm_timer_ptr list = search_list(timer->expires());
        timer_helper::timer_list_append(list, timer);
    }

    void update() {
        struct timeval tv;
        gettimeofday(&tv, nullptr);

        s64 tick_count = outdated_tick_count(&tv);
        while (tick_count > 0) {
            for (s64 i = 0; i < tick_count; ++i) {
                run_timer();
                ++current_tick_;
            }

            gettimeofday(&tv, nullptr);
            tick_count = outdated_tick_count(&tv);
        }

        timeval_add(tv, time_offset_, &current_time_);
    }

private:
    s64 outdated_tick_count(const struct timeval *tv) const {
        struct timeval diff;
        timeval_sub(*tv, start_time_, &diff);

        s64 real_tick = time2tick(&diff);
        if (likely(real_tick >= current_tick_)) return real_tick - current_tick_;

        sk_info("leap second! real<%lu>, current<%lu>.", real_tick, current_tick_);
        return 0;
    }

    shm_timer_ptr search_list(s64 expires) {
        sk_assert(expires >= current_tick_);
        s64 delta = expires - current_tick_;

        if (delta < TV1_MAX) return tv1_.vec[TV1_IDX(expires)];
        if (delta < TV2_MAX) return tv2_.vec[TV2_IDX(expires)];
        if (delta < TV3_MAX) return tv3_.vec[TV3_IDX(expires)];
        if (delta < TV4_MAX) return tv4_.vec[TV4_IDX(expires)];

        // if the timeout interval is larger than 0xFFFFFFFF
        // on 64-bit machine, then we use the maximum interval
        if (delta > 0xFFFFFFFFLL) {
            delta = 0xFFFFFFFFLL;
            expires = delta + current_tick_;
        }

        return tv5_.vec[TV5_IDX(expires)];
    }

    void cascade(shm_timer_ptr list) {
        shm_timer *head = list.get();
        while (!timer_helper::timer_list_empty(list)) {
            shm_timer_ptr ptr = head->next();
            shm_timer *timer = ptr.get();

            shm_timer_ptr new_list = search_list(timer->expires());
            timer_helper::timer_list_remove(ptr);
            timer_helper::timer_list_append(new_list, ptr);
        }
    }

    void run_timer() {
        s64 tv1_idx = TV1_IDX(current_tick_);
        do {
            if (tv1_idx != 0) break;

            s64 tv2_idx = TV2_IDX(current_tick_);
            cascade(tv2_.vec[tv2_idx]);
            if (tv2_idx != 0) break;

            s64 tv3_idx = TV3_IDX(current_tick_);
            cascade(tv3_.vec[tv3_idx]);
            if (tv3_idx != 0) break;

            s64 tv4_idx = TV4_IDX(current_tick_);
            cascade(tv4_.vec[tv4_idx]);
            if (tv4_idx != 0) break;

            s64 tv5_idx = TV5_IDX(current_tick_);
            cascade(tv5_.vec[tv5_idx]);
        } while (0);

        shm_timer_ptr list = tv1_.vec[tv1_idx];
        shm_timer *head = list.get();
        while (!timer_helper::timer_list_empty(list)) {
            shm_timer_ptr ptr = head->next();
            shm_timer *timer = ptr.get();
            sk_assert(timer->self() == ptr);

            // the timer is "running" before calling the registered callback,
            // then it must be that the process crashed when calling the callback
            // last time, so, just remove the "ill" timer
            if (timer->running()) {
                sk_fatal("timer callback crashed at last time, type<%d>.", timer->type());
                if (!timer->repeats()) {
                    timer_helper::timer_list_remove(ptr);
                    // TODO: what if the user calls shm_free() again if we have already
                    // destructed the timer here??
                    shm_timer::destruct(ptr);
                    continue;
                }
            }

            if (timer->repeats()) {
                timer->running_on();
                shm_timer_dispatcher::get()->on_timeout(ptr, timer->data(), timer->data_length());
                timer->running_off();
            }

            timer->refresh(current_tick_);
            if (timer->repeats()) {
                shm_timer_ptr new_list = search_list(timer->expires());
                timer_helper::timer_list_remove(ptr);
                timer_helper::timer_list_append(new_list, ptr);
                continue;
            }

            sk_info("timer finished, type<%d>.", timer->type());
            timer_helper::timer_list_remove(ptr);
            // TODO: what if the user calls shm_free() again if we have already
            // destructed the timer here??
            shm_timer::destruct(ptr);
        }
    }

    void on_tick(heap_timer *timer) {
        sk_assert(timer == tick_timer_.get());
        update();
    }

private:
    using timer_vector_node = sk::time::timer_vector<TVN_SIZE>;
    using timer_vector_root = sk::time::timer_vector<TVR_SIZE>;

    // BE CAREFUL, the following member should be
    // initialized after every process restarting
    std::unique_ptr<heap_timer> tick_timer_;

    s64 current_tick_;            // current tick
    struct timeval time_offset_;  // offset of real time and process time
    struct timeval start_time_;   // process start time
    struct timeval current_time_; // current process time

    timer_vector_root tv1_;
    timer_vector_node tv2_;
    timer_vector_node tv3_;
    timer_vector_node tv4_;
    timer_vector_node tv5_;
};
static_assert(std::is_standard_layout<shm_timer_mgr>::value, "invalid shm_timer_mgr");
static shm_timer_mgr *mgr = nullptr;

void shm_timer::stop() {
    if (running_) {
        repeats_ = 0;
        return;
    }

    timer_helper::timer_list_remove(self());
    destruct(self());
}

shm_timer_ptr shm_timer::start(s64 timeout_ms, s64 repeat_ms, int repeat_count,
                               int timer_type, const void *data, size_t len) {
    assert_retval(repeat_count == REPEAT_FOREVER || repeat_count >= 1, nullptr);

    if (!shm_timer_dispatcher::get()->has_callback(timer_type)) {
        sk_error("timer callback<%d> not registered.", timer_type);
        return nullptr;
    }

    size_t data_len = (data && len > 0) ? len : 0;
    shm_timer_ptr ptr = construct(data_len);
    assert_retval(ptr, nullptr);

    shm_timer *timer = ptr.get();
    timer->expires_    = mgr->current_tick() + ms2tick(timeout_ms);
    timer->interval_   = ms2tick(repeat_ms);
    timer->repeats_    = repeat_count;
    timer->timer_type_ = timer_type;
    timer->cb_len_     = data_len;

    timer->set_self_ptr(ptr);
    if (data_len > 0) memcpy(timer->cb_data_, data, data_len);

    mgr->add_timer(ptr);
    return ptr;
}

shm_timer_ptr shm_timer::construct(size_t extra_len) {
    size_t mem_len  = sizeof(shm_timer) + extra_len;
    shm_ptr<void> ptr = shm_malloc(mem_len);
    if (!ptr) return nullptr;

    new (ptr.get()) shm_timer();
    return shm_timer_ptr(ptr.address());
}

void shm_timer::destruct(shm_timer_ptr ptr) {
    ptr->~shm_timer();
    shm_free(shm_ptr<void>(ptr.address()));
}

int init(uv_loop_t *loop, int time_offset_sec) {
    shm_ptr<shm_timer_mgr> ptr = shm_get_singleton<shm_timer_mgr>(SHM_SINGLETON_SHM_TIMER_MGR, time_offset_sec);
    assert_retval(ptr, -ENOMEM);

    mgr = ptr.get();
    return mgr->init(loop);
}

void stop() {
    if (mgr) mgr->stop();
}

void fini() {
    shm_delete_singleton<shm_timer_mgr>(SHM_SINGLETON_SHM_TIMER_MGR);
}

int now() {
    return mgr->current_sec();
}

NS_END(time)
NS_END(sk)
