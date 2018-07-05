#include <map>
#include <set>
#include <sys/mman.h>
#include <sys/inotify.h>
#include <utility/utility.h>
#include <core/inet_address.h>
#include <utility/assert_helper.h>
#include <utility/system_helper.h>
#include <coroutine/detail/context.h>
#include <coroutine/detail/coroutine_mgr.h>

NS_BEGIN(sk)

static const int FLAG_PRESERVE_FPU  = 0x1;
static const int FLAG_PROTECT_STACK = 0x2;
static const int FLAG_TIMEOUT       = 0x4;

static size_t get_page_size() {
    static size_t page_size = 0;
    if (unlikely(page_size == 0)) {
        page_size = sk::get_sys_page_size();
    }

    return page_size;
}

enum coroutine_state {
    state_running,
    state_runnable,
    state_sleeping,
    state_io_waiting,
    state_cond_waiting,
    state_done
};

struct coroutine {
    coroutine(const std::string& name, const coroutine_function& fn,
              size_t stack_size, bool preserve_fpu, bool protect_stack) {
        sk_trace("coroutine::coroutine(%s)", name.c_str());

        this->flag       = 0;
        this->state      = state_runnable;
        this->ctx        = nullptr;
        this->stack      = nullptr;
        this->stack_size = stack_size;
        this->fn         = fn;
        snprintf(this->name, sizeof(this->name), "%s", name.c_str());

        if (preserve_fpu) {
            flag |= FLAG_PRESERVE_FPU;
        }

        if (protect_stack) {
            flag |= FLAG_PROTECT_STACK;
        }
    }

    ~coroutine() {
        sk_trace("coroutine::~coroutine(%s -> %d)", name, state);

        if (state != state_done) {
            sk_warn("coroutine(%s -> %d) is not done!", name, state);
        }

        if (stack) {
            if ((flag & FLAG_PROTECT_STACK) != 0) {
                const size_t page_size = get_page_size();
                const size_t mmap_size = stack_size + page_size * 2;
                munmap(stack, mmap_size);
            } else {
                free(stack);
            }

            stack = nullptr;
            stack_size = 0;

            // ctx actually points to an object on stack, so just reset it here
            ctx = nullptr;
        }
    }

    bool init() {
        void *top = nullptr;

        if ((flag & FLAG_PROTECT_STACK) != 0) {
            const size_t page_size = get_page_size();

            if ((stack_size & (page_size - 1)) > 0) {
                stack_size = (stack_size & ~(page_size - 1)) + page_size;
                sk_assert((stack_size & (page_size - 1)) == 0);
            }

            const size_t mmap_size = stack_size + page_size * 2;
            const int prot = PROT_READ | PROT_WRITE | PROT_EXEC;
            const int flags = MAP_PRIVATE | MAP_ANONYMOUS;

            stack = char_ptr(mmap(nullptr, mmap_size, prot, flags, -1, 0));
            if (stack == MAP_FAILED) {
                sk_error("mmap() error: %s.", strerror(errno));
                stack = nullptr; // MAP_FAILED is -1, we need to reset to null here
                return false;
            }

            uintptr_t ptr = reinterpret_cast<uintptr_t>(stack);
            sk_assert((ptr & (page_size - 1)) == 0);
            sk_assert(((ptr + mmap_size) & (page_size - 1)) == 0);

            if (mprotect(stack, page_size, PROT_NONE) == -1) {
                sk_error("mprotect() error: %s.", strerror(errno));
                munmap(stack, mmap_size);
                return false;
            }

            if (mprotect(stack + page_size + stack_size, page_size, PROT_NONE) == -1) {
                sk_error("mprotect() error: %s.", strerror(errno));
                munmap(stack, mmap_size);
                return false;
            }

            top = stack + page_size + stack_size;
        } else {
            stack = char_ptr(malloc(stack_size));
            if (!stack) {
                return false;
            }

            top = stack + stack_size;
        }

        ctx = detail::make_context(top, stack_size, detail::coroutine_mgr::context_main);
        return !!ctx;
    }

    int flag;
    int state;
    void *ctx;
    char *stack;
    char name[32];
    size_t stack_size;
    coroutine_function fn;
};

struct coroutine_handle {
    struct tcp_handle {
        tcp_handle() {
            retval = 0;
            listening = false;
            uv_listen_called = false;
            pending_connection_count = 0;
            incoming.buf = nullptr;
            incoming.len = 0;
            incoming.nbytes = 0;
            outgoing.nbytes = 0;
        }

        int retval;
        bool listening;
        bool uv_listen_called;
        size_t pending_connection_count;

        struct {
            void *buf;
            size_t len;
            ssize_t nbytes;
        } incoming;

        struct {
            ssize_t nbytes;
        } outgoing;
    };

    struct fs_watcher {
        fs_watcher() {
            ifd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
            sk_assert(ifd != -1);

            retval = 0;
            events = nullptr;
        }

        ~fs_watcher() {
            for (const auto& it : wd2file) {
                inotify_rm_watch(ifd, it.first);
            }

            wd2file.clear();
            file2wd.clear();

            if (ifd != -1) {
                close(ifd);
                ifd = -1;
            }
        }

        int ifd;
        int retval;
        std::map<int, std::string> wd2file;
        std::map<std::string, int> file2wd;
        std::vector<coroutine_fs_event> *events;
    };

    struct signal_watcher {
        signal_watcher() {
            sigset_t mask;
            sigemptyset(&mask);

            sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
            sk_assert(sfd != -1);
        }

        ~signal_watcher() {
            sigset_t mask;
            sigemptyset(&mask);

            for (const auto& sig : signals) {
                sigaddset(&mask, sig);
            }

            int ret = sigprocmask(SIG_UNBLOCK, &mask, nullptr);
            sk_assert(ret != -1);

            signals.clear();

            if (sfd != -1) {
                close(sfd);
                sfd = -1;
            }
        }

        int sfd;
        std::set<int> signals;
    };

    int type;
    sk::coroutine *coroutine;

    union U {
        tcp_handle tcp;
        fs_watcher fs;
        signal_watcher sig;

        // have to define user-defined ctor/dtor here, otherwise the code will not compile
        U() {}
        ~U() {}
    } u;

    union {
        uv_handle_t handle;
        uv_stream_t stream;
        uv_tcp_t tcp;
        uv_poll_t poll;
    } uv;

    coroutine_handle(int type, sk::coroutine *c, uv_loop_t *loop) : type(type), coroutine(c) {
        uv.handle.data = this;

        switch (type) {
        case coroutine_handle_tcp: {
            new (&u.tcp) tcp_handle();
            int ret = uv_tcp_init(loop, &uv.tcp);
            sk_assert(ret == 0);
            break;
        }
        case coroutine_handle_fs_watcher: {
            new (&u.fs) fs_watcher();
            int ret = uv_poll_init(loop, &uv.poll, u.fs.ifd);
            sk_assert(ret == 0);
            break;
        }
        case coroutine_handle_signal_watcher: {
            new (&u.sig) signal_watcher();
            int ret = uv_poll_init(loop, &uv.poll, u.sig.sfd);
            sk_assert(ret == 0);
            break;
        }
        default: {
            sk_error("invalid handle type: %d.", type);
        }
        }
    }

    ~coroutine_handle() {
        switch (type) {
        case coroutine_handle_tcp:
            u.tcp.~tcp_handle();
            break;
        case coroutine_handle_fs_watcher:
            u.fs.~fs_watcher();
            break;
        case coroutine_handle_signal_watcher:
            u.sig.~signal_watcher();
            break;
        default:
            sk_error("invalid handle type: %d.", type);
        }
    }
};

NS_BEGIN(detail)

static void *main_ctx = nullptr;
static coroutine_mgr *mgr = nullptr;

coroutine_mgr::coroutine_mgr(uv_loop_t *loop) : loop_(loop), uv_(nullptr), current_(nullptr) {
    uv_ = create("uv", [this] () {
        uv_run(loop_, UV_RUN_DEFAULT);
    }, 32 * 1024, false, true);

    // create(...) will push coroutine into the runnable_ queue, but
    // the uv_ coroutine should not be there, so we remove it manually
    sk_assert(runnable_.size() == 1);
    runnable_.pop();

    sk_assert(!mgr);
    mgr = this;
}

coroutine_mgr::~coroutine_mgr() {
    if (current_) {
        sk_error("coroutine_mgr::~coroutine_mgr() gets called inside a coroutine(%s)!", current_->name);
    }

    if (uv_) {
        sk_warn("uv_ is active!");
    }

    if (!runnable_.empty()) {
        sk_warn("there are still runnable coroutines!");
    }

    sk_assert(mgr);
    mgr = nullptr;
}

coroutine *coroutine_mgr::create(const std::string& name, const coroutine_function& fn,
                                 size_t stack_size, bool preserve_fpu, bool protect_stack) {
    coroutine *c = new coroutine(name, fn, stack_size, preserve_fpu, protect_stack);
    if (!c->init()) {
        delete c;
        return nullptr;
    }

    runnable_.push(c);
    return c;
}

uv_loop_t *coroutine_mgr::loop(){
    return loop_;
}

coroutine *coroutine_mgr::self() {
    return current_;
}

const char *coroutine_mgr::name() {
    return current_ ? current_->name : nullptr;
}

void coroutine_mgr::sleep(u64 ms) {
    sk_assert(current_ && current_->state == state_running);

    int ret = 0;
    uv_timer_t timer;
    timer.data = current_;

    ret = uv_timer_init(loop_, &timer);
    sk_assert(ret == 0);

    ret = uv_timer_start(&timer, on_sleep_timeout, ms, 0);
    sk_assert(ret == 0);

    current_->state = state_sleeping;
    yield(current_);
}

void coroutine_mgr::yield() {
    sk_assert(current_);

    // if current coroutine is the uv coroutine, just skip it
    if (unlikely(current_ == uv_)) {
        yield(current_);
        return;
    }

    switch (current_->state) {
        case state_running:
            sk_debug("coroutine(%s) yields under running state.", current_->name);
            current_->state = state_runnable;
            runnable_.push(current_);
            break;
        case state_runnable:
            runnable_.push(current_);
            break;
        case state_sleeping:
        case state_io_waiting:
        case state_cond_waiting:
        case state_done:
        default:
            break;
    }

    yield(current_);
}

void coroutine_mgr::schedule() {
    while (true) {
        if (runnable_.empty()) {
            uv_->state = state_running;
            current_ = uv_;
            resume(current_);
            current_ = nullptr;

            if (uv_->state == state_done) {
                delete uv_;
                uv_ = nullptr;
                break;
            }

            continue;
        }

        coroutine *c = runnable_.front();
        runnable_.pop();

        c->state = state_running;
        current_ = c;
        resume(c);
        current_ = nullptr;

        if (c->state == state_done) {
            sk_trace("coroutine(%s) done.", c->name);
            delete c;
        }
    }
}

coroutine *coroutine_mgr::get(coroutine_handle *h) {
    return h->coroutine;
}

void coroutine_mgr::bind(coroutine_handle *h, coroutine *c) {
    h->coroutine = c;
}

coroutine_handle *coroutine_mgr::handle_create(int type, coroutine *c) {
    sk_assert(c);
    return new coroutine_handle(type, c, loop_);
}

void coroutine_mgr::handle_close(coroutine_handle *h) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_);

    uv_close(&h->uv.handle, on_uv_close);
    current_->state = state_io_waiting;
    yield(current_);
}

void coroutine_mgr::handle_free(coroutine_handle *h) {
    delete h;
}

int coroutine_mgr::handle_type(coroutine_handle *h) {
    return h->type;
}

int coroutine_mgr::tcp_bind(coroutine_handle *h, u16 port) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_ && h->type == coroutine_handle_tcp);

    inet_address addr(port);
    return uv_tcp_bind(&h->uv.tcp, addr.address(), 0);
}

int coroutine_mgr::tcp_connect(coroutine_handle *h, const std::string& host, u16 port) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_ && h->type == coroutine_handle_tcp);

    uv_connect_t req;
    req.data = h;
    inet_address addr(host, port);

    int ret = uv_tcp_connect(&req, &h->uv.tcp, addr.address(), on_tcp_connect);
    if (ret != 0) {
        return ret;
    }

    h->u.tcp.retval = 0;
    current_->state = state_io_waiting;
    yield(current_);

    return h->u.tcp.retval;
}

int coroutine_mgr::tcp_listen(coroutine_handle *h, int backlog) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_ && h->type == coroutine_handle_tcp);

    int ret = 0;
    if (unlikely(!h->u.tcp.uv_listen_called)) {
        ret = uv_listen(&h->uv.stream, backlog, on_tcp_listen);
        if (ret != 0) {
            return ret;
        }

        h->u.tcp.uv_listen_called = true;
    }

    // there is pending connections, just return
    if (h->u.tcp.pending_connection_count > 0) {
        return 0;
    }

    h->u.tcp.retval = 0;
    h->u.tcp.listening = true;
    current_->state = state_io_waiting;
    yield(current_);

    sk_assert(h->u.tcp.pending_connection_count == 1);
    h->u.tcp.pending_connection_count -= 1;
    h->u.tcp.listening = false;
    return h->u.tcp.retval;
}

int coroutine_mgr::tcp_accept(coroutine_handle *h, coroutine_handle *client) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_ && h->type == coroutine_handle_tcp);
    return uv_accept(&h->uv.stream, &client->uv.stream);
}

int coroutine_mgr::tcp_shutdown(coroutine_handle *h) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_ && h->type == coroutine_handle_tcp);

    uv_shutdown_t req;
    req.data = h;

    int ret = uv_shutdown(&req, &h->uv.stream, on_tcp_shutdown);
    if (ret != 0) {
        return ret;
    }

    h->u.tcp.retval = 0;
    current_->state = state_io_waiting;
    yield(current_);

    return h->u.tcp.retval;
}

ssize_t coroutine_mgr::tcp_read(coroutine_handle *h, void *buf, size_t len) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_ && h->type == coroutine_handle_tcp);

    int ret = uv_read_start(&h->uv.stream, on_tcp_alloc, on_tcp_read);
    if (ret != 0) {
        return ret;
    }

    h->u.tcp.incoming.buf = buf;
    h->u.tcp.incoming.len = len;
    h->u.tcp.incoming.nbytes = 0;
    current_->state = state_io_waiting;
    yield(current_);

    ret = uv_read_stop(&h->uv.stream);
    sk_assert(ret == 0);

    return h->u.tcp.incoming.nbytes;
}

ssize_t coroutine_mgr::tcp_write(coroutine_handle *h, const void *buf, size_t len) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_ && h->type == coroutine_handle_tcp);

    uv_write_t req;
    req.data = h;

    uv_buf_t uv_buf;
    uv_buf.base = char_ptr(const_cast<void*>(buf));
    uv_buf.len = len;

    int ret = uv_write(&req, &h->uv.stream, &uv_buf, 1, on_tcp_write);
    if (ret != 0) {
        return ret;
    }

    h->u.tcp.outgoing.nbytes = len;
    current_->state = state_io_waiting;
    yield(current_);

    return h->u.tcp.outgoing.nbytes;
}

int coroutine_mgr::fs_add_watch(coroutine_handle *h, const std::string& file) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_ && h->type == coroutine_handle_fs_watcher);

    auto it = h->u.fs.file2wd.find(file);
    if (it != h->u.fs.file2wd.end()) {
        return 0;
    }

    int wd = inotify_add_watch(h->u.fs.ifd, file.c_str(), IN_ALL_EVENTS);
    if (wd == -1) {
        return -errno;
    }

    h->u.fs.file2wd[file] = wd;
    h->u.fs.wd2file[wd] = file;

    return 0;
}

int coroutine_mgr::fs_rm_watch(coroutine_handle *h, const std::string& file) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_ && h->type == coroutine_handle_fs_watcher);

    auto it = h->u.fs.file2wd.find(file);
    if (it == h->u.fs.file2wd.end()) {
        return 0;
    }

    int wd = it->second;
    int ret = inotify_rm_watch(h->u.fs.ifd, wd);
    if (ret != 0) {
        return ret;
    }

    h->u.fs.file2wd.erase(it);
    h->u.fs.wd2file.erase(wd);

    return 0;
}

int coroutine_mgr::fs_watch(coroutine_handle *h, std::vector<coroutine_fs_event> *events) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_ && h->type == coroutine_handle_fs_watcher);

    int ret = uv_poll_start(&h->uv.poll, UV_READABLE | UV_WRITABLE | UV_DISCONNECT, on_fs_event);
    if (ret != 0) {
        return ret;
    }

    h->u.fs.retval = 0;
    h->u.fs.events = events;
    h->u.fs.events->clear();
    current_->state = state_io_waiting;
    yield(current_);

    ret = uv_poll_stop(&h->uv.poll);
    sk_assert(ret == 0);

    return h->u.fs.retval;
}

void coroutine_mgr::context_main(intptr_t arg) {
    coroutine *c = reinterpret_cast<coroutine*>(arg);
    c->fn();

    c->state = state_done;
    yield(c);
}

void coroutine_mgr::yield(coroutine *c) {
    jump_context(&c->ctx, main_ctx, reinterpret_cast<intptr_t>(c), (c->flag & FLAG_PRESERVE_FPU) != 0);
}

void coroutine_mgr::resume(coroutine *c) {
    jump_context(&main_ctx, c->ctx, reinterpret_cast<intptr_t>(c), (c->flag & FLAG_PRESERVE_FPU) != 0);
}

void coroutine_mgr::on_sleep_timeout(uv_timer_t *handle) {
    coroutine *c = cast_ptr(coroutine, handle->data);
    sk_assert(c && c->state == state_sleeping);

    c->state = state_runnable;
    mgr->runnable_.push(c);

    sk_assert(mgr->current_ == mgr->uv_);
    yield(mgr->current_);
}

void coroutine_mgr::on_uv_close(uv_handle_t *handle) {
    coroutine_handle *h = cast_ptr(coroutine_handle, handle->data);
    sk_assert(h);

    coroutine *c = h->coroutine;
    sk_assert(c && c->state == state_io_waiting);

    c->state = state_runnable;
    mgr->runnable_.push(c);

    sk_assert(mgr->current_ == mgr->uv_);
    yield(mgr->current_);
}

void coroutine_mgr::on_tcp_connect(uv_connect_t *req, int status) {
    coroutine_handle *h = cast_ptr(coroutine_handle, req->data);
    sk_assert(h && h->type == coroutine_handle_tcp);

    coroutine *c = h->coroutine;
    sk_assert(c && c->state == state_io_waiting);

    h->u.tcp.retval = status;
    c->state = state_runnable;
    mgr->runnable_.push(c);

    sk_assert(mgr->current_ == mgr->uv_);
    yield(mgr->current_);
}

void coroutine_mgr::on_tcp_listen(uv_stream_t *server, int status) {
    coroutine_handle *h = cast_ptr(coroutine_handle, server->data);
    sk_assert(h && h->type == coroutine_handle_tcp);

    coroutine *c = h->coroutine;
    sk_assert(c);

    h->u.tcp.retval = status;
    h->u.tcp.pending_connection_count += 1; // TODO: shouldn't we check the status first?

    if (!h->u.tcp.listening) {
        sk_trace("coroutine(%s:%d) is not listening.", c->name, c->state);
        return;
    }

    sk_assert(c && c->state == state_io_waiting);
    c->state = state_runnable;
    mgr->runnable_.push(c);

    sk_assert(mgr->current_ == mgr->uv_);
    yield(mgr->current_);
}

void coroutine_mgr::on_tcp_shutdown(uv_shutdown_t *req, int status) {
    coroutine_handle *h = cast_ptr(coroutine_handle, req->data);
    sk_assert(h && h->type == coroutine_handle_tcp);

    coroutine *c = h->coroutine;
    sk_assert(c && c->state == state_io_waiting);

    h->u.tcp.retval = status;
    c->state = state_runnable;
    mgr->runnable_.push(c);

    sk_assert(mgr->current_ == mgr->uv_);
    yield(mgr->current_);
}

void coroutine_mgr::on_tcp_alloc(uv_handle_t *handle, size_t, uv_buf_t *buf) {
    coroutine_handle *h = cast_ptr(coroutine_handle, handle->data);
    sk_assert(h && h->type == coroutine_handle_tcp);

    buf->base = char_ptr(h->u.tcp.incoming.buf);
    buf->len = h->u.tcp.incoming.len;
}

void coroutine_mgr::on_tcp_read(uv_stream_t *stream, ssize_t nbytes, const uv_buf_t *buf) {
    coroutine_handle *h = cast_ptr(coroutine_handle, stream->data);
    sk_assert(h && h->type == coroutine_handle_tcp);

    coroutine *c = h->coroutine;
    sk_assert(c && c->state == state_io_waiting);
    sk_assert(h->u.tcp.incoming.buf == buf->base && h->u.tcp.incoming.len == buf->len);

    h->u.tcp.incoming.nbytes = nbytes;
    c->state = state_runnable;
    mgr->runnable_.push(c);

    sk_assert(mgr->current_ == mgr->uv_);
    yield(mgr->current_);
}

void coroutine_mgr::on_tcp_write(uv_write_t *req, int status) {
    coroutine_handle *h = cast_ptr(coroutine_handle, req->data);
    sk_assert(h && h->type == coroutine_handle_tcp);

    coroutine *c = h->coroutine;
    sk_assert(c && c->state == state_io_waiting);

    if (status < 0) {
        h->u.tcp.outgoing.nbytes = status;
    }

    c->state = state_runnable;
    mgr->runnable_.push(c);

    sk_assert(mgr->current_ == mgr->uv_);
    yield(mgr->current_);
}

void coroutine_mgr::on_fs_event(uv_poll_t *handle, int status, int events) {
    coroutine_handle *h = cast_ptr(coroutine_handle, handle->data);
    sk_assert(h && h->type == coroutine_handle_fs_watcher);

    coroutine *c = h->coroutine;
    sk_assert(c && c->state == state_io_waiting);

    if (status != 0) {
        h->u.fs.retval = status;
    } else {
        if (unlikely(events & UV_WRITABLE)) {
            sk_warn("writable events!");
        }

        if (unlikely(events & UV_DISCONNECT)) {
            sk_warn("disconnect events!");
        }

        if (events & UV_READABLE) {
            char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
            const struct inotify_event *event = nullptr;
            ssize_t len = 0;

            while (true) {
                do {
                    len = read(h->u.fs.ifd, buf, sizeof(buf));
                } while (len == -1 && errno == EINTR);

                if (len == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        sk_error("inotify read error: %s.", strerror(errno));
                    }

                    break;
                }

                if (len == 0) {
                    break;
                }

                for (char *ptr = buf; ptr < buf + len;
                     ptr += sizeof(struct inotify_event) + event->len) {
                    event = reinterpret_cast<const struct inotify_event *>(ptr);

                    if (event->mask == IN_IGNORED) {
                        continue;
                    }

                    auto it = h->u.fs.wd2file.find(event->wd);
                    if (it == h->u.fs.wd2file.end()) {
                        sk_warn("file watch<%d> not found.", event->wd);
                        continue;
                    }

                    sk_trace("OPEN(%d), CLOSE(%d), CHANGE(%d), DELETE(%d), file<%s>.",
                             event->mask & IN_OPEN ? 1 : 0, event->mask & IN_CLOSE ? 1 : 0,
                             event->mask & IN_MODIFY ? 1 : 0, event->mask & IN_DELETE_SELF ? 1 : 0,
                             it->second.c_str());

                    coroutine_fs_event e;
                    e.events = 0;
                    e.file = it->second;

                    if (event->mask & IN_OPEN) {
                        e.events |= coroutine_fs_event::event_open;
                    }

                    if (event->mask & IN_MODIFY) {
                        e.events |= coroutine_fs_event::event_change;
                    }

                    if (event->mask & IN_CLOSE) {
                        e.events |= coroutine_fs_event::event_close;
                    }

                    if (event->mask & IN_DELETE_SELF) {
                        e.events |= coroutine_fs_event::event_delete;
                    }

                    h->u.fs.events->push_back(std::move(e));
                }
            }
        }
    }

    c->state = state_runnable;
    mgr->runnable_.push(c);

    sk_assert(mgr->current_ == mgr->uv_);
    yield(mgr->current_);
}

NS_END(detail)
NS_END(sk)
