#include <libsk.h>

#define print_log(fmt, ...)  printf(fmt, ##__VA_ARGS__);printf("\n");sk_debug(fmt, ##__VA_ARGS__)

int main() {
    int ret = sk::logger::init("log.xml");
    if (ret != 0) return ret;

    sk::coroutine_init(uv_default_loop());

    sk::coroutine_create("file-watcher", [] () {
        sk::coroutine_handle *h = sk::coroutine_handle_create(sk::coroutine_handle_fs_watcher);
        int ret = sk::coroutine_fs_add_watch(h, "log.xml");
        sk_assert(ret == 0);

        print_log("now start to watch");
        std::vector<sk::coroutine_fs_event> events;
        while (true) {
            ret = sk::coroutine_fs_watch(h, &events);
            sk_assert(ret == 0);

            bool exit = false;
            for (const auto& it : events) {
                print_log("file: %s, events: open(%d), close(%d), change(%d), delete(%d).", it.file.c_str(),
                          (it.events & sk::coroutine_fs_event::event_open) ? 1 : 0,
                          (it.events & sk::coroutine_fs_event::event_close) ? 1 : 0,
                          (it.events & sk::coroutine_fs_event::event_change) ? 1 : 0,
                          (it.events & sk::coroutine_fs_event::event_delete) ? 1 : 0);

                if (it.events & sk::coroutine_fs_event::event_close) {
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

    sk::coroutine_create("file-operator", [] () {
        sk::coroutine_sleep(2000);
        int fd = open("log.xml", O_RDONLY);
        if (fd != -1) {
            print_log("file log.xml opened.");
            sk::coroutine_sleep(2000);
            close(fd);
            print_log("file log.xml closed.");
        }
    });

    sk::coroutine_schedule();
    sk::coroutine_fini();

    sk_debug("main done.");
    return 0;
}
