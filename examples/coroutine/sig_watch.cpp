#include <libsk.h>

#define print_log(fmt, ...)  printf(fmt, ##__VA_ARGS__);printf("\n");sk_debug(fmt, ##__VA_ARGS__)

int main() {
    int ret = sk::logger::init("log.xml");
    if (ret != 0) return ret;

    sk::coroutine_init(uv_default_loop());

    sk::coroutine_create("sig-watcher", [] () {
        int ret = 0;
        sk::coroutine_handle *h = sk::coroutine_handle_create(sk::coroutine_handle_signal_watcher);

        ret = sk::coroutine_sig_add_watch(h, SIGUSR1);
        sk_assert(ret == 0);

        ret = sk::coroutine_sig_add_watch(h, SIGUSR2);
        sk_assert(ret == 0);

        print_log("now start to watch");
        std::vector<signalfd_siginfo> signals;
        while (true) {
            ret = sk::coroutine_sig_watch(h, &signals);
            sk_assert(ret == 0);

            bool exit = false;
            for (const auto& it : signals) {
                const char *sig = "N/A";
                switch (it.ssi_signo) {
                    case SIGUSR1:
                        sig = "SIGUSR1";
                        break;
                    case SIGUSR2:
                        sig = "SIGUSR2";
                        break;
                }

                print_log("ssi_signo: %s, ssi_pid: %u, ssi_int: %d, ssi_ptr: %lu",
                          sig, it.ssi_pid, it.ssi_int, it.ssi_ptr);

                if (it.ssi_signo == SIGUSR2) {
                    exit = true;
                }
            }

            if (exit) {
                break;
            }
        }

        sk::coroutine_handle_close(h);
        sk::coroutine_handle_free(h);
    });

    sk::coroutine_create("sig-sender", [] () {
        sk::coroutine_sleep(1000);
        pid_t pid = getpid();
        sigval v;

        print_log("kill(pid, SIGUSR1)...");
        kill(pid, SIGUSR1);
        sk::coroutine_sleep(1000);

        print_log("sigqueue(pid, SIGUSR1, 7)...");
        v.sival_int = 7;
        sigqueue(pid, SIGUSR1, v);
        sk::coroutine_sleep(1000);

        print_log("sigqueue(pid, SIGUSR1, ptr)...");
        v.sival_ptr = &v;
        sigqueue(pid, SIGUSR1, v);
        sk::coroutine_sleep(1000);

        print_log("kill(pid, SIGUSR2)...");
        kill(pid, SIGUSR2);
    });

    sk::coroutine_schedule();
    sk::coroutine_fini();

    sk_debug("main done.");
    return 0;
}
