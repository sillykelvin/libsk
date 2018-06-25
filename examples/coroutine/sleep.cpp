#include <coroutine/coroutine.h>
#include <libsk.h>

int main() {
    int ret = sk::logger::init("log.xml");
    if (ret != 0) return ret;

    sk::coroutine_init(uv_default_loop());

    sk::coroutine::create([]() {
        for (int i = 0; i < 10; ++i) {
            printf("==> sleep1 - %d\n", i);
            sk_debug("==> sleep1 - %d", i);
            sk::coroutine::current_coroutine()->sleep(1000);
            printf("<== sleep1 - %d\n", i);
            sk_debug("<== sleep1 - %d", i);
        }
    });

    sk::coroutine::create([]() {
        for (int i = 0; i < 10; ++i) {
            printf("==> sleep2 - %d\n", i);
            sk_debug("==> sleep2 - %d", i);
            sk::coroutine::current_coroutine()->sleep(1000);
            printf("<== sleep2 - %d\n", i);
            sk_debug("<== sleep2 - %d", i);
        }
    });

    sk::coroutine_schedule();
    sk::coroutine_fini();

    sk_debug("main done.");
    return 0;
}
