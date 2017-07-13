#ifndef SERVER_H
#define SERVER_H

#include <memory>
#include <fcntl.h>
#include <signal.h>
#include <bus/bus.h>
#include <log/log.h>
#include <sys/file.h>
#include <time/time.h>
#include <shm/shm_mgr.h>
#include <spdlog/spdlog.h>
#include <core/heap_timer.h>
#include <core/signal_watcher.h>
#include <server/option_parser.h>

NS_BEGIN(sk)

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
    bool resume_mode;      // if the process is running under resume mode
    bool disable_shm;      // disable shared memory manager explicitly
    bool disable_bus;      // disable bus explicitly
    int bus_key;           // shm key of bus, if disable_bus is true, this one is useless
    size_t bus_node_size;  // bus node size, if disable_bus is true, this one is useless
    size_t bus_node_count; // bus node count, if disable_bus is true, this one is useless

    // do NOT touch the following fields unless you know what you are doing

    // if hotfixing is true, main loop will exit directly, there must be
    // a monitor process to resume this process
    bool hotfixing;

    server_context() :
        id(0),
        resume_mode(false),
        disable_shm(false),
        disable_bus(false),
        bus_key(0),
        bus_node_size(0),
        bus_node_count(0),
        hotfixing(false)
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
    using this_type = server<Config, Derived>;

    MAKE_NONCOPYABLE(server);

    static const size_t MAX_MSG_PER_PROC = 1000; // process 1000 messages in one run

    static Derived& get() {
        if (likely(instance_))
            return *instance_;

        instance_ = new Derived();
        return *instance_;
    }

    virtual ~server() = default;

    int start(int argc, const char **argv) {
        int ret = 0;

        ret = init(argc, argv);
        if (ret != 0) return ret;

        run();

        ret = fini();
        if (ret != 0) return ret;

        return 0;
    }

    const Config& config() const { return cfg_; }
    const server_context& context() const { return ctx_; }
    uv_loop_t *loop() const { return loop_; }
    sk::signal_watcher *signal_watcher() const { return sig_watcher_; }

private:
    int init(int argc, const char **argv) {
        int ret = 0;

        option_parser p;
        ret = init_parser(p);
        if (ret != 0) return ret;

        ret = p.parse(argc, argv, nullptr);
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

        ret = make_daemon();
        if (ret != 0) return ret;

        ret = lock_pid();
        if (ret != 0) return ret;

        ret = write_pid();
        if (ret != 0) return ret;

        ret = register_bus();
        if (ret != 0) return ret;

        ret = create_loop();
        if (ret != 0) return ret;

        ret = watch_signal();
        if (ret != 0) return ret;

        ret = on_init();
        if (ret != 0) return ret;

        if (sk::time::time_enabled(nullptr)) {
            ret = start_tick_timer(1000);
            if (ret != 0) return ret;
        }

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

    void run() {
        sk_info("enter run()");
        uv_run(loop_, UV_RUN_DEFAULT);
        sk_info("exit run()");
    }

    int fini() {
        sk_info("enter fini()");
        int ret = 0;

        do {
            if (ctx_.hotfixing) {
                sk_info("hotfixing, exit.");
                break;
            }

            ret = on_fini();

            if (buf_) {
                free(buf_);
                buf_ = nullptr;
                buf_len_ = 0;
            }

            if (sig_watcher_) {
                delete sig_watcher_;
                sig_watcher_ = nullptr;
            }

            if (!ctx_.disable_bus)
                sk::bus::deregister_bus();

            if (!ctx_.disable_shm)
                sk::shm_mgr_fini();
        } while (0);

        sk_info("exit fini()");
        return ret;
    }

    void stop() {
        sk_info("enter stop()");
        sk_assert(!stop_timer_);

        sig_watcher_->stop();
        if (tick_timer_) tick_timer_->stop();

        stop_timer_ = std::unique_ptr<heap_timer>(new heap_timer(loop_,
                                                                 std::bind(&this_type::on_stop_timeout,
                                                                           this, std::placeholders::_1)));
        if (stop_timer_) {
            stop_timer_->start_once(0);
            return;
        }

        sk_error("cannot create timer.");
        uv_stop(loop_);
        sk_info("exit stop()");
    }

    void reload() {
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

        sk_info("exit reload()");
    }

    void hotfix() {
        sk_info("enter hotfix()");
        ctx_.hotfixing = true;
        uv_stop(loop_);
        sk_info("exit hotfix()");
    }

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

    virtual int on_reload() = 0;

    virtual void on_msg(int src_busid, const void *buf, size_t len) = 0;

    /**
     * @brief on_signal will be called when there is a registered signal is caught
     * @param info: the signal information
     *
     * NOTE: the derived class should override this function if it registered
     * own signals with sig_watcher_, otherwise, no override is needed
     */
    virtual void on_signal(const signalfd_siginfo *info) {}

    /**
     * @brief on_tick will be called every fixed milliseconds, the interval
     * is specified at start_tick_timer(...) call
     *
     * NOTE: this function will never be called if the tick timer is not enabled,
     * see @start_tick_timer() for more information
     */
    virtual void on_tick() {}

    /**
     * @brief start_tick_timer will start the tick timer
     * @param tick_ms: the interval(in ms) that the tick timer will run
     * @return 0 if started successfully, error otherwise
     *
     * NOTE: if the derived class initializes own time manager with
     * sk::time::init_time(...) in on_init(...) call, the base class
     * will call this function to start a tick timer which runs every
     * 1 second to update the time manager's current time, however,
     * the derived class can also call this function to start the tick
     * timer, but if the time manager is enabled, it's better to start
     * the tick timer with a interval <= 1 second.
     */
    int start_tick_timer(u64 tick_ms) {
        if (tick_timer_) return 0;

        tick_timer_ = std::unique_ptr<heap_timer>(new heap_timer(loop_,
                                                                 std::bind(&this_type::on_tick_timeout,
                                                                           this, std::placeholders::_1)));
        if (!tick_timer_) return -ENOMEM;

        tick_timer_->start_forever(tick_ms, tick_ms);
        return 0;
    }

private:
    void recv_bus_msg() {
        assert_retnone(!ctx_.disable_bus);

        int ret = 0;
        size_t total = 0;
        while (total < MAX_MSG_PER_PROC) {
            int src_busid = -1;
            size_t len = buf_len_;
            ret = sk::bus::recv(src_busid, buf_, len);

            if (ret == 0)
                break;

            if (unlikely(ret < 0)) {
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
    }

    void handle_signal(const signalfd_siginfo *info) {
        int signal = static_cast<int>(info->ssi_signo);
        switch (signal) {
        case SIGPIPE:
            break;
        case SIGTERM:
        case SIGQUIT:
        case SIGINT:
            stop();
            break;
        case SIGUSR1:
            reload();
            break;
        case SIGUSR2:
            hotfix();
            break;
        default:
            if (signal == bus::BUS_INCOMING_SIGNO) {
                recv_bus_msg();
                break;
            }

            return on_signal(info);
        }
    }

    void on_stop_timeout(heap_timer *timer) {
        sk_assert(timer == stop_timer_.get());

        int ret = on_stop();
        if (ret == 0) {
            // do NOT call stop here, let the loop quit
            // automatically after all handles get closed
            // uv_stop(loop_);
            sk_info("exit stop()");
            return;
        }

        sk_info("on_stop() returns %d, continue.", ret);
        stop_timer_->start_once(500);
    }

    void on_tick_timeout(heap_timer *timer) {
        sk_assert(timer == tick_timer_.get());

        if (sk::time::time_enabled(nullptr))
            sk::time::update_time();

        return on_tick();
    }

private:
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

        ret = p.register_option(0, "resume", "process start in resume mode or not", nullptr, false, &ctx_.resume_mode);
        if (ret != 0) return ret;

        ret = p.register_option(0, "disable-shm", "do NOT use shm", nullptr, false, &ctx_.disable_shm);
        if (ret != 0) return ret;

        ret = p.register_option(0, "disable-bus", "do NOT use bus", nullptr, false, &ctx_.disable_bus);
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

    int init_conf() {
        return cfg_.load_from_xml_file(ctx_.proc_conf.c_str());
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

    int make_daemon() {
        return daemon(1, 0);
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

    int register_bus() {
        if (ctx_.disable_bus)
            return 0;

        return sk::bus::register_bus(ctx_.bus_key, ctx_.id, ctx_.bus_node_size, ctx_.bus_node_count);
    }

    int create_loop() {
        loop_ = uv_default_loop();
        if (!loop_) return -1;
        return 0;
    }

    int watch_signal() {
        sig_watcher_ = signal_watcher::create(loop_);
        if (!sig_watcher_) return -1;

        int ret = 0;
        sig_watcher_->set_signal_callback(std::bind(&this_type::handle_signal,
                                                    this, std::placeholders::_1));

        // NOTE: it's important to ignore SIGPIPE
        sk_info("signal: SIGPIPE(%d), action: ignore", SIGPIPE);
        ret = sig_watcher_->watch(SIGPIPE);
        if (ret != 0) return ret;

        sk_info("signal: SIGTERM(%d), action: stop", SIGTERM);
        ret = sig_watcher_->watch(SIGTERM);
        if (ret != 0) return ret;

        sk_info("signal: SIGQUIT(%d), action: stop", SIGQUIT);
        ret = sig_watcher_->watch(SIGQUIT);
        if (ret != 0) return ret;

        sk_info("signal: SIGINT(%d), action: stop", SIGINT);
        ret = sig_watcher_->watch(SIGINT);
        if (ret != 0) return ret;

        sk_info("signal: SIGUSR1(%d), action: reload", SIGUSR1);
        ret = sig_watcher_->watch(SIGUSR1);
        if (ret != 0) return ret;

        sk_info("signal: SIGUSR2(%d), action: hotfix", SIGUSR2);
        ret = sig_watcher_->watch(SIGUSR2);
        if (ret != 0) return ret;

        sk_info("signal: BUS_INCOMING(%d), action: recv", sk::bus::BUS_INCOMING_SIGNO);
        ret = sig_watcher_->watch(sk::bus::BUS_INCOMING_SIGNO);
        if (ret != 0) return ret;

        return sig_watcher_->start();
    }

private:
    static Derived *instance_;

    void *buf_;
    size_t buf_len_;
    Config cfg_;
    server_context ctx_;
    uv_loop_t *loop_;
    sk::signal_watcher *sig_watcher_;
    std::unique_ptr<heap_timer> stop_timer_;
    std::unique_ptr<heap_timer> tick_timer_;
};

template<typename C, typename D>
D *server<C, D>::instance_ = nullptr;

NS_END(sk)

#endif // SERVER_H
