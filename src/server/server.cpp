#include <libgen.h>
#include "server.h"
#include "server/option_parser.h"

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

namespace sk {

template<typename Config>
int server<Config>::init(int argc, const char **argv) {
    int ret = 0;

    ret = init_parser();
    if (ret != 0) return ret;

    ret = parser_.parse(argc, argv, NULL);
    if (ret != 0) return ret;

    ret = init_ctx(basename(argv[0]));
    if (ret != 0) return ret;

    ret = init_logger();
    if (ret != 0) return ret;

    ret = init_conf();
    if (ret != 0) return ret;

    // TODO: add more initialization here

    ret = on_init();
    if (ret != 0) return ret;

    return 0;
}

template<typename Config>
int server<Config>::init_parser() {
    int ret = 0;

    ret = parser_.register_option(0, "id", "id of this process", "x.x.x.x", true, &ctx_.str_id);
    if (ret != 0) return ret;

    ret = parser_.register_option(0, "pid-file", "pid file location", "PID", false, &ctx_.pid_file);
    if (ret != 0) return ret;

    ret = parser_.register_option(0, "log-conf", "log config location", "CONF", false, &ctx_.log_conf);
    if (ret != 0) return ret;

    ret = parser_.register_option(0, "proc-conf", "process config location", "CONF", false, &ctx_.proc_conf);
    if (ret != 0) return ret;

    ret = parser_.register_option(0, "resume", "process start in resume mode or not", NULL, false, &ctx_.resume_mode);
    if (ret != 0) return ret;

    return 0;
}

template<typename Config>
int server<Config>::init_ctx(const char *program) {
    assert_retval(!ctx_.str_id.empty(), -1);

    busid_format f;
    int ret = sscanf(ctx_.str_id.c_str(), "%d.%d.%d.%d",
                     static_cast<int*>(&f.area_id),
                     static_cast<int*>(&f.zone_id),
                     static_cast<int*>(&f.func_id),
                     static_cast<int*>(&f.inst_id));
    if (ret != 4)
        return ret;

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

    return 0;
}

template<typename Config>
int server<Config>::init_logger() {
    // TODO: rewrite this function

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_sink_st>());
    sinks.push_back(std::make_shared<spdlog::sinks::daily_file_sink_st>("server.log", "txt", 23, 59));
    logger_ = std::make_shared<spdlog::logger>("server", sinks.begin(), sinks.end());

    return 0;
}

template<typename Config>
int server<Config>::lock_pid() {
    int fd = open(ctx_.pid_file.c_str(), O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        error("cannot open pid file %s, error: %d.", ctx_.pid_file.c_str(), errno);
        return -1;
    }

    int ret = flock(fd, LOCK_EX | LOCK_NB);
    if (ret == 0)
        return ret;

    close(fd);
    error("cannot lock pid file %s.", ctx_.pid_file.c_str());
    return -1;
}

template<typename Config>
int server<Config>::write_pid() {
    // TODO: implement this function
    return 0;
}

} // namespace sk
