#ifndef SERVER_H
#define SERVER_H

#include "server/option_parser.h"

namespace sk {

struct server_context {
    u64 id;               // id of this server, it is also the bus id
    std::string str_id;   // string id of this server, like "x.x.x.x"
    std::string pid_file; // pid file location
    bool resume_mode;     // if the process is running under resume mode

    server_context() : id(0), resume_mode(false) {}
};

template<typename Config>
class server {
public:
    virtual ~server() {}

    int init(int argc, const char **argv);
    int fini();
    int stop();
    int run();
    int reload();
    void usage();

    const Config& config() const { return conf_; }
    const server_context& ctx() const { return ctx_; }

protected:
    virtual int on_init() = 0;
    virtual int on_fini() = 0;
    virtual int on_stop() = 0;
    virtual int on_run() = 0;
    virtual int on_reload() = 0;

private:
    int init_parser();
    int init_ctx(const char *program);

private:
    Config conf_;
    server_context ctx_;
    option_parser parser_;
};

} // namespace sk

#endif // SERVER_H
