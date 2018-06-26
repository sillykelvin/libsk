#include <libsk.h>

int main() {
    int ret = sk::logger::init("log.xml");
    if (ret != 0) return ret;

    sk::coroutine_init(uv_default_loop());

    sk::coroutine_create("sleep-1", [] () {
        for (int i = 0; i < 10; ++i) {
            printf("==> sleep1 - %d\n", i);
            sk_debug("==> sleep1 - %d", i);
            sk::coroutine_sleep(1000);
            printf("<== sleep1 - %d\n", i);
            sk_debug("<== sleep1 - %d", i);
        }
    }, 8 * 1024, false, true);

    sk::coroutine_create("sleep-2", [] () {
        for (int i = 0; i < 10; ++i) {
            printf("==> sleep2 - %d\n", i);
            sk_debug("==> sleep2 - %d", i);
            sk::coroutine_sleep(1000);
            printf("<== sleep2 - %d\n", i);
            sk_debug("<== sleep2 - %d", i);
        }
    }, 8 * 1024, false, true);

    sk::coroutine_schedule();
    sk::coroutine_fini();

    sk_debug("main done.");
    return 0;
}
