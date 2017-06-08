#include <queue>
#include "time/time.h"
#include "heap_timer.h"
#include "utility/assert_helper.h"

#define REPEAT_FOREVER -1

using namespace sk::time;

class heap_timer_mgr {
public:
    static heap_timer_ptr new_timer(u32 first_interval_sec,
                                    u32 repeat_interval_sec,
                                    int repeat_count,
                                    const heap_timer_handler& handler) {
        return heap_timer_ptr(new heap_timer(first_interval_sec,
                                             repeat_interval_sec,
                                             repeat_count, handler));
    }

    void add_timer(const heap_timer_ptr& timer) {
        assert_retnone(timer);
        timer_queue_.push(timer);
    }

    void run_timer() {
        u64 current_tick = sk::time::current_tick();
        while (!timer_queue_.empty()) {
            const heap_timer_ptr& top = timer_queue_.top();
            if (top->expires_ > current_tick)
                break;

            if (top->cancelled()) {
                timer_queue_.pop();
                continue;
            }

            // do a copy here to avoid shared_ptr destruction
            // after timer_queue_.pop() gets called
            heap_timer_ptr timer(top);
            timer_queue_.pop();

            timer->running_ = true;
            if (timer->repeats_ > 0 || timer->repeats_ == REPEAT_FOREVER) {
                if (timer->repeats_ > 0)
                    --timer->repeats_;

                timer->handler_(timer);
            }
            timer->running_ = false;

            if (timer->repeats_ > 0 || timer->repeats_ == REPEAT_FOREVER) {
                timer->expires_ = current_tick + timer->interval_;
                add_timer(timer);
                continue;
            }
        }
    }

private:
    struct timer_comparator {
        bool operator()(const heap_timer_ptr& lhs,
                        const heap_timer_ptr& rhs) const {
            assert_retval(lhs, true);
            assert_retval(rhs, false);

            if (lhs->expires_ != rhs->expires_)
                return lhs->expires_ < rhs->expires_;

            return true;
        }
    };

    using timer_queue = std::priority_queue<heap_timer_ptr,
                                            std::vector<heap_timer_ptr>,
                                            timer_comparator>;
    timer_queue timer_queue_;
};
static heap_timer_mgr *mgr = nullptr;

NS_BEGIN(sk)
NS_BEGIN(time)

heap_timer::heap_timer(u32 first_interval_sec, u32 repeat_interval_sec,
                       int repeat_count, const heap_timer_handler& handler)
    : cancelled_(false),
      running_(false),
      repeats_(repeat_count),
      expires_(current_tick() + sec2tick(first_interval_sec)),
      interval_(sec2tick(repeat_interval_sec)),
      handler_(handler)
{}

heap_timer_ptr add_timer(u32 first_interval_sec, u32 repeat_interval_sec,
                         int repeat_count, const heap_timer_handler& handler) {
    assert_retval(repeat_count == REPEAT_FOREVER || repeat_count >= 1, nullptr);

    bool ok = time_enabled(nullptr);
    assert_retval(ok, nullptr);

    if (repeat_interval_sec == 0 && repeat_count == REPEAT_FOREVER)
        assert_retval(0, nullptr);

    auto ptr = heap_timer_mgr::new_timer(first_interval_sec,
                                         repeat_interval_sec,
                                         repeat_count, handler);
    mgr->add_timer(ptr);
    return ptr;
}

heap_timer_ptr add_timer(u32 interval_sec, int repeat_count,
                         const heap_timer_handler& handler) {
    return add_timer(interval_sec, interval_sec, repeat_count, handler);
}

heap_timer_ptr add_forever_timer(u32 first_interval_sec,
                                 u32 repeat_interval_sec,
                                 const heap_timer_handler& handler) {
    return add_timer(first_interval_sec, repeat_interval_sec, REPEAT_FOREVER, handler);
}

heap_timer_ptr add_forever_timer(u32 interval_sec, const heap_timer_handler& handler) {
    return add_timer(interval_sec, interval_sec, REPEAT_FOREVER, handler);
}

heap_timer_ptr add_single_timer(u32 interval_sec, const heap_timer_handler& handler) {
    return add_timer(interval_sec, 0, 1, handler);
}

bool heap_timer_enabled() {
    return mgr != nullptr;
}

void run_heap_timer() {
    mgr->run_timer();
}

int init_heap_timer() {
    if (!time_enabled(nullptr))
        return -EINVAL;

    mgr = new heap_timer_mgr();
    assert_retval(mgr, -ENOMEM);
    return 0;
}

NS_END(time)
NS_END(sk)
