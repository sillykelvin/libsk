#ifndef SERVER_H
#define SERVER_H

#include <memory>
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include "spdlog/spdlog.h"
#include "server/option_parser.h"
#include "utility/log.h"

NS_BEGIN(sk)
NS_BEGIN(detail)

union busid_format {
    u64 busid;
    struct {
        u16 area_id;
        u16 zone_id;
        u16 func_id;
        u16 inst_id;
    };
};
static_assert(sizeof(busid_format) == sizeof(u64), "invalid type: busid_format");

NS_END(detail)

struct server_context {
    u64 id;                // id of this server, it is also the bus id
    std::string str_id;    // string id of this server, like "x.x.x.x"
    std::string pid_file;  // pid file location
    std::string log_conf;  // log config location
    std::string proc_conf; // process config location
    int max_idle_count;    // max idle count allowed
    int idle_sleep_sec;    // how long server will sleep when idle
    bool resume_mode;      // if the process is running under resume mode

    // do NOT touch the following fields unless you know what you are doing

    // if reloading is true, reload() will be called in next loop
    bool reloading;
    // if stopping is true, stop() will be called in main loop regularly,
    // to do clean up job
    bool stopping;
    // if clean up job is done, set exiting to true to break the main loop
    bool exiting;

    int idle_count; // current idle count

    server_context() :
        id(0),
        max_idle_count(0),
        idle_sleep_sec(0),
        resume_mode(false),
        reloading(false),
        stopping(false),
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

        ret = make_daemon();
        if (ret != 0) return ret;

        /* from now we can use logging functions */

        ret = init_conf();
        if (ret != 0) return ret;

        ret = lock_pid();
        if (ret != 0) return ret;

        ret = write_pid();
        if (ret != 0) return ret;

        set_signal_handler();

        // TODO: add more initialization here

        ret = on_init();
        if (ret != 0) return ret;

        return 0;
    }

    int fini() {
        return on_fini();
    }

    int stop() {
        int ret = on_stop();
        if (ret == 0) {
            ctx_.exiting = true;
            return 0;
        }

        sk_info("on_stop() returns %d, continue running.", ret);
        return 0;
    }

    int run() {
        sk_info("enter run()");

        while (!ctx_.exiting) {
            if (ctx_.stopping)
                stop();

            if (ctx_.reloading)
                reload();

            int ret = on_run();
            if (ret == 0) {
                ++ctx_.idle_count;
                if (ctx_.idle_count >= ctx_.max_idle_count) {
                    ctx_.idle_count = 0;
                    usleep(ctx_.idle_sleep_sec * 1000);
                }
            }
        }

        sk_info("exit run()");
        return 0;
    }

    int reload() {
        // TODO: reload config here
        int ret = on_reload();
        if (ret != 0)
            sk_error("on_reload returns error: %d", ret);

        ctx_.reloading = false;
        return ret;
    }

    const Config& config() const { return conf_; }
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
     * @return 0 means it is an idle run, server will increase idle count,
     * if idle count reaches the configured count, server will sleep for
     * some time until next run, otherwise server will call on_run() again
     * without any pause
     */
    virtual int on_run() = 0;
    virtual int on_reload() = 0;

private:
    static void sig_on_stop(int) {
        instance_->ctx_.stopping = true;
    }

    static void sig_on_reload(int) {
        instance_->ctx_.reloading = true;
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

        ret = p.register_option(0, "idle-sleep", "how long server will sleep when idle, in sec", "SEC", false, &ctx_.idle_sleep_sec);
        if (ret != 0) return ret;

        ret = p.register_option(0, "resume", "process start in resume mode or not", NULL, false, &ctx_.resume_mode);
        if (ret != 0) return ret;

        return 0;
    }

    int init_context(const char *program) {
        assert_retval(!ctx_.str_id.empty(), -1);

        int area_id = 0, zone_id = 0, func_id = 0, inst_id = 0;
        int ret = sscanf(ctx_.str_id.c_str(), "%d.%d.%d.%d",
                         &area_id, &zone_id, &func_id, &inst_id);
        if (ret != 4)
            return ret;

        detail::busid_format f;
        f.area_id = area_id;
        f.zone_id = zone_id;
        f.func_id = func_id;
        f.inst_id = inst_id;
        ctx_.id = f.busid;

        // the command line option does not provide a pid file
        if (ctx_.pid_file.empty()) {
            char buf[256] = {0};
            snprintf(buf, sizeof(buf), "/tmp/%s_%s.pid", program, ctx_.str_id.c_str());
            ctx_.pid_file = buf;
        }

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
        if (ctx_.idle_sleep_sec == 0) {
            ctx_.idle_sleep_sec = 3;
        }

        return 0;
    }

    int init_logger() {
        return sk::logger::init(ctx_.log_conf);
    }

    int make_daemon() {
        return daemon(1, 0);
    }

    int init_conf() {
        return conf_.load_from_xml_file(ctx_.proc_conf.c_str());
    }

    int lock_pid() {
        int fd = open(ctx_.pid_file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
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
        int fd = open(ctx_.pid_file.c_str(), O_RDWR);
        if (fd == -1) {
            sk_error("cannot open pid file %s, error: %s.", ctx_.pid_file.c_str(), strerror(errno));
            return -errno;
        }

        pid_t pid = getpid();
        ssize_t len = write(fd, &pid, sizeof(pid));
        if (len != sizeof(pid)) {
            sk_error("cannot write pid, error: %s.", strerror(errno));
            return -errno;
        }

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

        sk_info("signal hander set.");
    }

private:
    static Derived *instance_;

    Config conf_;
    server_context ctx_;
    std::shared_ptr<spdlog::logger> logger_;
};

template<typename C, typename D>
D *server<C, D>::instance_ = NULL;

NS_END(sk)

#endif // SERVER_H
