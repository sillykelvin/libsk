#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <memory>
#include "utility/types.h"

class heap_timer_mgr;

NS_BEGIN(sk)
NS_BEGIN(time)

class heap_timer;
using heap_timer_ptr = std::shared_ptr<heap_timer>;
using heap_timer_handler = std::function<void(const heap_timer_ptr&)>;

class heap_timer {
public:
    void cancel() { cancelled_ = true; }
    bool cancelled() const { return cancelled_; }

private:
    heap_timer(u32 first_interval_sec, u32 repeat_interval_sec,
               int repeat_count, const heap_timer_handler& handler);

private:
    bool cancelled_;             // true if the timer is cancelled
    bool running_;               // true if the timer is running
    int repeats_;                // repeat count, forever if it's -1
    u64 expires_;                // tick to next timeout
    u64 interval_;               // timeout interval, in tick
    heap_timer_handler handler_; // timeout callback

    friend class ::heap_timer_mgr;
};

heap_timer_ptr add_timer(u32 first_interval_sec, u32 repeat_interval_sec,
                         int repeat_count, const heap_timer_handler& handler);

heap_timer_ptr add_timer(u32 interval_sec, int repeat_count,
                         const heap_timer_handler& handler);

heap_timer_ptr add_forever_timer(u32 first_interval_sec,
                                 u32 repeat_interval_sec,
                                 const heap_timer_handler& handler);

heap_timer_ptr add_forever_timer(u32 interval_sec,
                                 const heap_timer_handler& handler);

heap_timer_ptr add_single_timer(u32 interval_sec,
                                const heap_timer_handler& handler);

bool heap_timer_enabled();
void run_heap_timer();
int init_heap_timer();

NS_END(time)
NS_END(sk)

#endif // HEAP_TIMER_H
