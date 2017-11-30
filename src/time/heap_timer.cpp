#include <string.h>
#include <log/log.h>
#include <time/heap_timer.h>
#include <utility/assert_helper.h>

NS_BEGIN(sk)

heap_timer::heap_timer(uv_loop_t *loop, const fn_on_timeout& fn)
    : fn_on_timeout_(fn) {
    memset(&timer_, 0x00, sizeof(timer_));
    timer_.timer.data = this;
    int ret = uv_timer_init(loop, &timer_.timer);
    sk_assert(ret == 0);
}

heap_timer::~heap_timer() {
    if (!stopped()) {
        sk_warn("~heap_timer(): active timer!");
        stop();
    }
}

void heap_timer::start_once(u64 timeout_ms) {
    start_forever(timeout_ms, 0);
}

void heap_timer::start_forever(u64 timeout_ms, u64 repeat_ms) {
    if (!stopped()) {
        sk_warn("restarting active timer!");
        stop();
    }

    int ret = uv_timer_start(&timer_.timer, on_timeout, timeout_ms, repeat_ms);
    if (ret != 0) sk_error("uv_timer_start: %s", uv_err_name(ret));
}

void heap_timer::stop() {
    int ret = uv_timer_stop(&timer_.timer);
    if (ret != 0) sk_error("uv_timer_stop: %s", uv_err_name(ret));
}

void heap_timer::close(const fn_on_timer_close& fn) {
    fn_on_close_ = fn;
    uv_close(&timer_.handle, on_close);
}

void heap_timer::on_timeout(uv_timer_t *handle) {
    heap_timer *t = cast_ptr(heap_timer, handle->data);
    sk_assert(&t->timer_.timer == handle);
    return t->fn_on_timeout_(t);
}

void heap_timer::on_close(uv_handle_t *handle) {
    heap_timer *t = cast_ptr(heap_timer, handle->data);
    sk_assert(&t->timer_.handle == handle);
    if (t->fn_on_close_) t->fn_on_close_(t);
}

NS_END(sk)
