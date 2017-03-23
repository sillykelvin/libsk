#ifndef SERVER_H
#define SERVER_H

#include <memory>
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include "bus/bus.h"
#include "spdlog/spdlog.h"
#include "server/option_parser.h"
#include "log/log.h"
#include "shm/shm_mgr.h"
#include "time/timer.h"

namespace sk {

inline std::string default_pid_file(int area_id, int zone_id,
                                    int func_id, int inst_id,
                                    const char *program) {
    char buf[256] = {0};
    snprintf(buf, sizeof(buf), "/tmp/%s_%d.%d.%d.%d.pid",
             program, area_id, zone_id, func_id, inst_id);
    return buf;
}

struct server_context {
    int id;                // id of this server, it is also the bus id & shm key
    std::string str_id;    // string id of this server, like "x.x.x.x"
    std::string pid_file;  // pid file location
    std::string log_conf;  // log config location
    std::string proc_conf; // process config location
    int max_idle_count;    // max idle count allowed
    int idle_sleep_ms;     // how long server will sleep when idle
    bool resume_mode;      // if the process is running under resume mode
    bool disable_shm;      // disable shared memory manager explicitly
    bool disable_bus;      // disable bus explicitly
    int bus_key;           // shm key of bus, if disable_bus is true, this one is useless
    size_t bus_node_size;  // bus node size, if disable_bus is true, this one is useless
    size_t bus_node_count; // bus node count, if disable_bus is true, this one is useless

    // do NOT touch the following fields unless you know what you are doing

    // if reloading is true, reload() will be called in next loop
    bool reloading;
    // if stopping is true, stop() will be called in main loop regularly,
    // to do clean up job
    bool stopping;
    // if hotfixing is true, main loop will exit directly, there must be
    // a monitor process to resume this process
    bool hotfixing;
    // if clean up job is done, set exiting to true to break the main loop
    bool exiting;

    int idle_count; // current idle count

    server_context() :
        id(0),
        max_idle_count(0),
        idle_sleep_ms(0),
        resume_mode(false),
        disable_shm(false),
        disable_bus(false),
        bus_key(0),
        bus_node_size(0),
        bus_node_count(0),
        reloading(false),
        stopping(false),
        hotfixing(false),
        exiting(false),
        idle_count(0)
    {}
};

/**
 * @brief The server class represents a server process
 * Note:
 *     1. Config is a configuration type, must have a
 * function load_from_xml_file(const char *filename)
 *     2. Derived must be the derived type from server
 * class
 */
template<typename Config, typename Derived>
class server {
public:
    MAKE_NONCOPYABLE(server);

    static const size_t MAX_MSG_PER_PROC = 1000; // process 1000 messages in one run

    static Derived& get() {
        if (instance_)
            return *instance_;

        instance_ = new Derived();
        return *instance_;
    }

    virtual ~server() = default;

    int init(int argc, const char **argv) {
        int ret = 0;

        option_parser p;
        ret = init_parser(p);
        if (ret != 0) return ret;

        ret = p.parse(argc, argv, NULL);
        if (ret != 0) return ret;

        ret = init_context(basename(argv[0]));
        if (ret != 0) return ret;

        ret = init_logger();
        if (ret != 0) return ret;

        /* from now we can use logging functions */

        ret = init_conf();
        if (ret != 0) return ret;

        ret = init_buf();
        if (ret != 0) return ret;

        ret = init_shm();
        if (ret != 0) return ret;

        ret = init_bus();
        if (ret != 0) return ret;

        ret = make_daemon();
        if (ret != 0) return ret;

        ret = lock_pid();
        if (ret != 0) return ret;

        ret = write_pid();
        if (ret != 0) return ret;

        set_signal_handler();

        ret = on_init();
        if (ret != 0) return ret;

        /*
         * in guid.cpp, we create a guid based on current
         * time and self-increased serial, however, if we
         * hotfixed the process, we will lost the stored
         * last serial, if the time is not changed before
         * and after hotfix (in second), the serial will
         * start from 0 again, this will lead to duplicate
         * guid, that's not what we want, so we wait for
         * one second here to enuse that after hotfixed
         * time is different from before hotfix, so that
         * the newly created guids will not duplicate.
         */
        sleep(1);

        return 0;
    }

    int fini() {
        sk_info("enter fini()");

        if (ctx_.hotfixing) {
            sk_info("hotfixing, exit.");
            sk_info("exit fini()");
            return 0;
        }

        int ret = on_fini();

        if (buf_) {
            free(buf_);
            buf_ = NULL;
            buf_len_ = 0;
        }

        if (!ctx_.disable_bus)
            sk::bus::deregister_bus(ctx_.bus_key, ctx_.id);

        if (!ctx_.disable_shm)
            sk::shm_mgr_fini();

        sk_info("exit fini()");

        return ret;
    }

    int stop() {
        sk_info("enter stop()");
        int ret = on_stop();
        if (ret == 0) {
            ctx_.exiting = true;
            sk_info("exit stop()");
            return 0;
        }

        sk_info("on_stop() returns %d, continue running.", ret);
        return 0;
    }

    int hotfix() {
        sk_info("enter hotfix()");
        ctx_.exiting = true;
        sk_info("exit hotfix()");
        return 0;
    }

    int run() {
        sk_info("enter run()");

        while (!ctx_.exiting) {
            if (ctx_.hotfixing)
                hotfix();

            if (ctx_.stopping)
                stop();

            if (ctx_.reloading)
                reload();

            if (sk::time::timer_enabled())
                sk::time::run_timer();

            bool bus_idle = true;
            if (!ctx_.disable_bus) {
                int ret = 0;
                size_t total = 0;
                while (total < MAX_MSG_PER_PROC) {
                    int src_busid = -1;
                    size_t len = buf_len_;
                    ret = sk::bus::recv(src_busid, buf_, len);

                    if (ret == 0)
                        break;

                    if (ret < 0) {
                        if (ret == -E2BIG) {
                            sk_warn("big msg, size<%lu>.", len);

                            void *old = buf_;
                            buf_ = malloc(len);
                            if (!buf_) {
                                sk_assert(0);
                                buf_ = old;
                                break;
                            }

                            free(old);
                            buf_len_ = len;
                            continue;
                        }

                        sk_error("bus recv error<%d>.", ret);
                        break;
                    }

                    sk_assert(ret == 1);
                    on_msg(src_busid, buf_, len);
                    ++total;
                }

                if (ret != 0)
                    bus_idle = false;
            }

            int ret = on_run();
            if (ret == 0 && bus_idle) {
                ++ctx_.idle_count;
                if (ctx_.idle_count >= ctx_.max_idle_count) {
                    ctx_.idle_count = 0;
                    usleep(ctx_.idle_sleep_ms * 1000);
                }
            }
        }

        sk_info("exit run()");
        return 0;
    }

    int reload() {
        sk_info("enter reload()");

        int ret = 0;

        Config tmp;
        ret = tmp.load_from_xml_file(ctx_.proc_conf.c_str());
        if (ret != 0)
            sk_error("reload proc conf error<%d>.", ret);
        else
            cfg_ = tmp;

        ret = sk::logger::reload();
        if (ret != 0)
            sk_error("reload logger error<%d>.", ret);

        ret = on_reload();
        if (ret != 0)
            sk_error("on_reload returns error: %d", ret);

        ctx_.reloading = false;

        sk_info("exit reload()");
        return ret;
    }

    const Config& config() const { return cfg_; }
    const server_context& context() const { return ctx_; }

protected:
    server() = default;

    virtual int on_init() = 0;
    virtual int on_fini() = 0;

    /**
     * @brief on_stop will be called when server receives a stop signal
     * @return 0 means server can shutdown, otherwise server will continue
     * to run and call on_stop() until it returns 0
     */
    virtual int on_stop() = 0;

    /**
     * @brief on_run will be called in every main loop
     * @return 0 means it is an idle run, server will increase idle count
     * if on_run() returns 0 and bus is idle, if idle count reaches the
     * configured count, server will sleep for some time until next run,
     * otherwise server will call on_run() again without any pause
     */
    virtual int on_run() = 0;
    virtual int on_reload() = 0;

    virtual void on_msg(int src_busid, const void *buf, size_t len) = 0;

private:
    static void sig_on_stop(int) {
        instance_->ctx_.stopping = true;
    }

    static void sig_on_reload(int) {
        instance_->ctx_.reloading = true;
    }

    static void sig_on_hotfix(int) {
        instance_->ctx_.hotfixing = true;
    }

    int init_parser(option_parser& p) {
        int ret = 0;

        ret = p.register_option(0, "id", "id of this process", "x.x.x.x", true, &ctx_.str_id);
        if (ret != 0) return ret;

        ret = p.register_option(0, "pid-file", "pid file location", "PID", false, &ctx_.pid_file);
        if (ret != 0) return ret;

        ret = p.register_option(0, "log-conf", "log config location", "CONF", false, &ctx_.log_conf);
        if (ret != 0) return ret;

        ret = p.register_option(0, "proc-conf", "process config location", "CONF", false, &ctx_.proc_conf);
        if (ret != 0) return ret;

        ret = p.register_option(0, "idle-count", "how many idle count will be counted before a sleep", "COUNT", false, &ctx_.max_idle_count);
        if (ret != 0) return ret;

        ret = p.register_option(0, "idle-sleep", "how long server will sleep when idle, in milliseconds", "MS", false, &ctx_.idle_sleep_ms);
        if (ret != 0) return ret;

        ret = p.register_option(0, "resume", "process start in resume mode or not", NULL, false, &ctx_.resume_mode);
        if (ret != 0) return ret;

        ret = p.register_option(0, "disable-shm", "do NOT use shm", NULL, false, &ctx_.disable_shm);
        if (ret != 0) return ret;

        ret = p.register_option(0, "disable-bus", "do NOT use bus", NULL, false, &ctx_.disable_bus);
        if (ret != 0) return ret;

        ret = p.register_option(0, "bus-key", "shm key of bus, 1799 by default", "KEY", false, &ctx_.bus_key);
        if (ret != 0) return ret;

        ret = p.register_option(0, "bus-node-size", "bus node size of bus, 128 by default", "SIZE", false, &ctx_.bus_node_size);
        if (ret != 0) return ret;

        ret = p.register_option(0, "bus-node-count", "bus node count of bus, 102400 by default", "COUNT", false, &ctx_.bus_node_count);
        if (ret != 0) return ret;

        return 0;
    }

    int init_context(const char *program) {
        assert_retval(!ctx_.str_id.empty(), -1);

        int area_id, zone_id, func_id, inst_id;
        ctx_.id = sk::bus::from_string(ctx_.str_id.c_str(),
                                       &area_id, &zone_id,
                                       &func_id, &inst_id);
        if (ctx_.id == -1) return -EINVAL;

        // the command line option does not provide a pid file
        if (ctx_.pid_file.empty())
            ctx_.pid_file = default_pid_file(area_id, zone_id,
                                             func_id, inst_id,
                                             program);

        // the command line option does not provide a log config
        if (ctx_.log_conf.empty()) {
            char buf[256] = {0};
            snprintf(buf, sizeof(buf), "../cfg/log_conf.xml_%s", ctx_.str_id.c_str());
            ctx_.log_conf = buf;
        }

        // the command line option does not provide a process config
        if (ctx_.proc_conf.empty()) {
            char buf[256] = {0};
            snprintf(buf, sizeof(buf), "../cfg/%s_conf.xml_%s", program, ctx_.str_id.c_str());
            ctx_.proc_conf = buf;
        }

        // the command line option does not provide an idle count
        if (ctx_.max_idle_count == 0) {
            ctx_.max_idle_count = 10;
        }

        // the command line option does not provide an idle sleep
        if (ctx_.idle_sleep_ms == 0) {
            ctx_.idle_sleep_ms = 3;
        }

        // the command line option does not provide a bus key
        if (ctx_.bus_key == 0) {
            ctx_.bus_key = sk::bus::DEFAULT_BUS_KEY;
        }

        // the command line option does not provide a bus node size
        if (ctx_.bus_node_size == 0) {
            ctx_.bus_node_size = sk::bus::DEFAULT_BUS_NODE_SIZE;
        }

        // the command line option does not provide a bus node count
        if (ctx_.bus_node_count == 0) {
            ctx_.bus_node_count = sk::bus::DEFAULT_BUS_NODE_COUNT;
        }

        return 0;
    }

    int init_logger() {
        return sk::logger::init(ctx_.log_conf);
    }

    int init_buf() {
        buf_len_ = 1 * 1024 * 1024; // 1MB
        buf_ = malloc(buf_len_);
        if (buf_) return 0;

        buf_len_ = 0;
        return -ENOMEM;
    }

    int init_shm() {
        if (cfg_.shm_size <= 0)
            ctx_.disable_shm = true;

        if (ctx_.disable_shm)
            return 0;

        return sk::shm_mgr_init(ctx_.id, cfg_.shm_size, ctx_.resume_mode);
    }

    int init_bus() {
        if (ctx_.disable_bus)
            return 0;

        return sk::bus::register_bus(ctx_.bus_key, ctx_.id, ctx_.bus_node_size, ctx_.bus_node_count);
    }

    int make_daemon() {
        return daemon(1, 0);
    }

    int init_conf() {
        return cfg_.load_from_xml_file(ctx_.proc_conf.c_str());
    }

    int lock_pid() {
        int fd = open(ctx_.pid_file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fd == -1) {
            sk_error("cannot open pid file %s, error: %s.", ctx_.pid_file.c_str(), strerror(errno));
            return -errno;
        }

        int ret = flock(fd, LOCK_EX | LOCK_NB);
        if (ret != 0) {
            close(fd);
            sk_error("cannot lock pid file %s, error: %s.", ctx_.pid_file.c_str(), strerror(errno));
            return -errno;
        }

        // do NOT close(fd) here
        return 0;
    }

    int write_pid() {
        int fd = open(ctx_.pid_file.c_str(), O_RDWR | O_TRUNC);
        if (fd == -1) {
            sk_error("cannot open pid file %s, error: %s.", ctx_.pid_file.c_str(), strerror(errno));
            return -errno;
        }

        char buf[32] = {0};
        snprintf(buf, sizeof(buf) - 1, "%d", getpid());
        size_t slen = strlen(buf);
        ssize_t wlen = write(fd, buf, slen);
        if (wlen != static_cast<ssize_t>(slen)) {
            sk_error("cannot write pid, error: %s.", strerror(errno));
            close(fd);
            return -errno;
        }

        close(fd);
        return 0;
    }

    void set_signal_handler() {
        struct sigaction act;

        sk_info("setting signal handler...");

        memset(&act, 0x00, sizeof(act));
        sigfillset(&act.sa_mask);

        act.sa_handler = server::sig_on_stop;
        sigaction(SIGTERM, &act, NULL);
        sk_info("signal: SIGTERM(%d), action: call server::stop", SIGTERM);

        sigaction(SIGQUIT, &act, NULL);
        sk_info("signal: SIGQUIT(%d), action: call server::stop", SIGQUIT);

        sigaction(SIGINT, &act, NULL);
        sk_info("signal: SIGINT(%d), action: call server::stop", SIGINT);

        sigaction(SIGABRT, &act, NULL);
        sk_info("signal: SIGABRT(%d), action: call server::stop", SIGABRT);

        act.sa_handler = server::sig_on_reload;
        sigaction(SIGUSR1, &act, NULL);
        sk_info("signal: SIGUSR1(%d), action: call server::reload", SIGUSR1);

        act.sa_handler = server::sig_on_hotfix;
        sigaction(SIGUSR2, &act, NULL);
        sk_info("signal: SIGUSR2(%d), action: call server::hotfix", SIGUSR2);

        sk_info("signal hander set.");
    }

private:
    static Derived *instance_;

    void *buf_;
    size_t buf_len_;
    Config cfg_;
    server_context ctx_;
};

template<typename C, typename D>
D *server<C, D>::instance_ = NULL;

} // namespace sk

#endif // SERVER_H
