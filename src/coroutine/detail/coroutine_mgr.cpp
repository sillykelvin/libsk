#include <sys/mman.h>
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

struct coroutine_tcp_handle {
    int retval;
    bool listening;
    bool uv_listen_called;
    sk::coroutine *coroutine;
    size_t pending_connection_count;

    struct {
        void *buf;
        size_t len;
        ssize_t nbytes;
    } incoming;

    struct {
        ssize_t nbytes;
    } outgoing;

    union {
        uv_handle_t handle;
        uv_stream_t stream;
        uv_tcp_t tcp;
    } uv;
};

NS_BEGIN(detail)

static void *main_ctx = nullptr;
static coroutine_mgr *mgr = nullptr;

coroutine_mgr::coroutine_mgr(uv_loop_t *loop) : loop_(loop), uv_(nullptr), current_(nullptr) {
    uv_ = create("uv", [this] () {
        uv_run(loop_, UV_RUN_DEFAULT);
    }, 16 * 1024, false, true);

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

    heap_timer *t = new heap_timer(loop_, std::bind(on_sleep_timeout, current_, std::placeholders::_1));
    t->start_once(ms);

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

coroutine *coroutine_mgr::get(coroutine_tcp_handle *h) {
    return h->coroutine;
}

void coroutine_mgr::bind(coroutine_tcp_handle *h, coroutine *c) {
    h->coroutine = c;
}

coroutine_tcp_handle *coroutine_mgr::tcp_create(coroutine *c) {
    sk_assert(c);

    coroutine_tcp_handle *h = cast_ptr(coroutine_tcp_handle, malloc(sizeof(coroutine_tcp_handle)));
    if (!h) {
        return nullptr;
    }

    int ret = uv_tcp_init(loop_, &h->uv.tcp);
    if (ret != 0) {
        free(h);
        return nullptr;
    }

    h->retval = 0;
    h->listening = false;
    h->uv_listen_called = false;
    h->coroutine = c;
    h->pending_connection_count = 0;
    h->incoming.buf = nullptr;
    h->incoming.len = 0;
    h->incoming.nbytes = 0;
    h->outgoing.nbytes = 0;
    h->uv.handle.data = h;

    // TODO: call uv_tcp_nodelay()/keepalive() here?
    return h;
}

void coroutine_mgr::tcp_free(coroutine_tcp_handle *h) {
    free(h);
}

int coroutine_mgr::tcp_bind(coroutine_tcp_handle *h, u16 port) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_);

    inet_address addr(port);
    return uv_tcp_bind(&h->uv.tcp, addr.address(), 0);
}

int coroutine_mgr::tcp_connect(coroutine_tcp_handle *h, const std::string& host, u16 port) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_);

    uv_connect_t req;
    req.data = h;
    inet_address addr(host, port);

    int ret = uv_tcp_connect(&req, &h->uv.tcp, addr.address(), on_tcp_connect);
    if (ret != 0) {
        return ret;
    }

    h->retval = 0;
    current_->state = state_io_waiting;
    yield(current_);

    return h->retval;
}

int coroutine_mgr::tcp_listen(coroutine_tcp_handle *h, int backlog) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_);

    int ret = 0;
    if (unlikely(!h->uv_listen_called)) {
        ret = uv_listen(&h->uv.stream, backlog, on_tcp_listen);
        if (ret != 0) {
            return ret;
        }

        h->uv_listen_called = true;
    }

    // there is pending connections, just return
    if (h->pending_connection_count > 0) {
        return 0;
    }

    h->retval = 0;
    h->listening = true;
    current_->state = state_io_waiting;
    yield(current_);

    sk_assert(h->pending_connection_count == 1);
    h->pending_connection_count -= 1;
    h->listening = false;
    return h->retval;
}

int coroutine_mgr::tcp_accept(coroutine_tcp_handle *h, coroutine_tcp_handle *client) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_);
    return uv_accept(&h->uv.stream, &client->uv.stream);
}

int coroutine_mgr::tcp_shutdown(coroutine_tcp_handle *h) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_);

    uv_shutdown_t req;
    req.data = h;

    int ret = uv_shutdown(&req, &h->uv.stream, on_tcp_shutdown);
    if (ret != 0) {
        return ret;
    }

    h->retval = 0;
    current_->state = state_io_waiting;
    yield(current_);

    return h->retval;
}

void coroutine_mgr::tcp_close(coroutine_tcp_handle *h) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_);

    uv_close(&h->uv.handle, on_uv_close);
    current_->state = state_io_waiting;
    yield(current_);
}

ssize_t coroutine_mgr::tcp_read(coroutine_tcp_handle *h, void *buf, size_t len) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_);

    int ret = uv_read_start(&h->uv.stream, on_tcp_alloc, on_tcp_read);
    if (ret != 0) {
        return ret;
    }

    h->incoming.buf = buf;
    h->incoming.len = len;
    h->incoming.nbytes = 0;
    current_->state = state_io_waiting;
    yield(current_);

    ret = uv_read_stop(&h->uv.stream);
    sk_assert(ret == 0);

    return h->incoming.nbytes;
}

ssize_t coroutine_mgr::tcp_write(coroutine_tcp_handle *h, const void *buf, size_t len) {
    sk_assert(current_ && current_->state == state_running && h->coroutine == current_);

    uv_write_t req;
    req.data = h;

    uv_buf_t uv_buf;
    uv_buf.base = char_ptr(const_cast<void*>(buf));
    uv_buf.len = len;

    int ret = uv_write(&req, &h->uv.stream, &uv_buf, 1, on_tcp_write);
    if (ret != 0) {
        return ret;
    }

    h->outgoing.nbytes = len;
    current_->state = state_io_waiting;
    yield(current_);

    return h->outgoing.nbytes;
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

void coroutine_mgr::on_sleep_timeout(coroutine *c, heap_timer *t) {
    sk_assert(c->state == state_sleeping);

    c->state = state_runnable;
    mgr->runnable_.push(c);

    sk_assert(mgr->current_ == mgr->uv_);
    yield(mgr->current_);

    t->close([] (heap_timer *t) {
        delete t;
    });
}

void coroutine_mgr::on_tcp_connect(uv_connect_t *req, int status) {
    coroutine_tcp_handle *h = cast_ptr(coroutine_tcp_handle, req->data);
    coroutine *c = h->coroutine;
    sk_assert(c && c->state == state_io_waiting);

    h->retval = status;
    c->state = state_runnable;
    mgr->runnable_.push(c);

    sk_assert(mgr->current_ == mgr->uv_);
    yield(mgr->current_);
}

void coroutine_mgr::on_tcp_listen(uv_stream_t *server, int status) {
    coroutine_tcp_handle *h = cast_ptr(coroutine_tcp_handle, server->data);
    coroutine *c = h->coroutine;

    h->retval = status;
    h->pending_connection_count += 1; // TODO: shouldn't we check the status first?

    if (!h->listening) {
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
    coroutine_tcp_handle *h = cast_ptr(coroutine_tcp_handle, req->data);
    coroutine *c = h->coroutine;
    sk_assert(c && c->state == state_io_waiting);

    h->retval = status;
    c->state = state_runnable;
    mgr->runnable_.push(c);

    sk_assert(mgr->current_ == mgr->uv_);
    yield(mgr->current_);
}

void coroutine_mgr::on_uv_close(uv_handle_t *handle) {
    coroutine_tcp_handle *h = cast_ptr(coroutine_tcp_handle, handle->data);
    coroutine *c = h->coroutine;
    sk_assert(c && c->state == state_io_waiting);

    c->state = state_runnable;
    mgr->runnable_.push(c);

    sk_assert(mgr->current_ == mgr->uv_);
    yield(mgr->current_);
}

void coroutine_mgr::on_tcp_alloc(uv_handle_t *handle, size_t, uv_buf_t *buf) {
    coroutine_tcp_handle *h = cast_ptr(coroutine_tcp_handle, handle->data);
    buf->base = char_ptr(h->incoming.buf);
    buf->len = h->incoming.len;
}

void coroutine_mgr::on_tcp_read(uv_stream_t *stream, ssize_t nbytes, const uv_buf_t *buf) {
    coroutine_tcp_handle *h = cast_ptr(coroutine_tcp_handle, stream->data);
    coroutine *c = h->coroutine;
    sk_assert(c && c->state == state_io_waiting);
    sk_assert(h->incoming.buf == buf->base && h->incoming.len == buf->len);

    h->incoming.nbytes = nbytes;
    c->state = state_runnable;
    mgr->runnable_.push(c);

    sk_assert(mgr->current_ == mgr->uv_);
    yield(mgr->current_);
}

void coroutine_mgr::on_tcp_write(uv_write_t *req, int status) {
    coroutine_tcp_handle *h = cast_ptr(coroutine_tcp_handle, req->data);
    coroutine *c = h->coroutine;
    sk_assert(c && c->state == state_io_waiting);

    if (status < 0) {
        h->outgoing.nbytes = status;
    }

    c->state = state_runnable;
    mgr->runnable_.push(c);

    sk_assert(mgr->current_ == mgr->uv_);
    yield(mgr->current_);
}

NS_END(detail)
NS_END(sk)
