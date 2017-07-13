#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <uv.h>
#include <core/callback.h>

NS_BEGIN(sk)

class heap_timer {
public:
    MAKE_NONCOPYABLE(heap_timer);

    heap_timer(uv_loop_t *loop, const fn_on_timeout& fn);
    ~heap_timer();

    void start_once(u64 timeout_ms);
    void start_forever(u64 timeout_ms, u64 repeat_ms);
    void stop();

    /**
     * @brief close the timer handle
     * @param fn: the callback will be called after handle closed
     *
     * NOTE: this function is optional, for those pre-allocated
     * timers, this function call is not needed, for those timers
     * allocated in heap, we cannot delete them in the timeout
     * callback (doing this will lead to memory corruption), so
     * this function should called to close the handle and then
     * delete the timer in the callback provided as parameter
     */
    void close(const fn_on_timer_close& fn);

    uv_handle_t *handle() { return &timer_.handle; }
    bool stopped() const { return !uv_is_active(&timer_.handle); }

private:
    static void on_timeout(uv_timer_t *handle);
    static void on_close(uv_handle_t *handle);

private:
    union {
        uv_handle_t handle;
        uv_timer_t timer;
    } timer_;
    fn_on_timeout fn_on_timeout_;
    fn_on_timer_close fn_on_close_;
};

NS_END(sk)

#endif // HEAP_TIMER_H
