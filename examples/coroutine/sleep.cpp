#include <coroutine/coroutine.h>
#include <libsk.h>

int main() {
    int ret = sk::logger::init("log.xml");
    if (ret != 0) return ret;

    sk::coroutine_init(uv_default_loop());

    sk::coroutine::create([]() {
        sk_trace("aaa1");
        sk::coroutine::current_coroutine()->sleep(1000);
        sk_trace("aaa2");
    }, 1024 * 1024);

    sk::coroutine::create([]() {
        sk_trace("bbb1");
        sk::coroutine::current_coroutine()->sleep(1000);
        sk_trace("bbb2");
    }, 1024 * 1024);

    sk::coroutine_schedule();
    sk::coroutine_fini();

    sk_debug("main done.");
    return 0;
}
