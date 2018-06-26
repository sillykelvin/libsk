#include <utility/assert_helper.h>
#include <coroutine/detail/context.h>
#include <coroutine/detail/coroutine_mgr.h>

NS_BEGIN(sk)

static const int FLAG_PRESERVE_FPU = 0x1;
static const int FLAG_TIMEOUT      = 0x2;

enum coroutine_state {
    state_running,
    state_runnable,
    state_sleeping,
    state_io_waiting,
    state_cond_waiting,
    state_done
};

struct coroutine {
    coroutine(const std::string& name,
              const coroutine_function& fn,
              size_t stack_size, bool preserve_fpu) {
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
    }

    ~coroutine() {
        sk_trace("coroutine::~coroutine(%s -> %d)", name, state);

        if (state != state_done) {
            sk_warn("coroutine(%s -> %d) is not done!", name, state);
        }

        if (stack) {
            free(stack);
            stack = nullptr;

            // ctx actually points to an object on stack, so just reset it here
            ctx = nullptr;
        }
    }

    bool init() {
        // TODO: use mprotect(...) to avoid stack overflow here
        stack = char_ptr(malloc(stack_size));
        if (!stack) {
            return false;
        }

        ctx = detail::make_context(stack + stack_size, stack_size, detail::coroutine_mgr::context_main);
        if (!ctx) {
            return false;
        }

        return true;
    }

    int flag;
    int state;
    void *ctx;
    char *stack;
    char name[32];
    size_t stack_size;
    coroutine_function fn;
};

NS_BEGIN(detail)

static void *main_ctx = nullptr;

coroutine_mgr::coroutine_mgr(uv_loop_t *loop) : loop_(loop), uv_(nullptr), current_(nullptr) {
    uv_ = create("uv", [this] () {
        uv_run(loop_, UV_RUN_DEFAULT);
    }, 8 * 1024, false);

    // create(...) will push coroutine into the runnable_ queue, but
    // the uv_ coroutine should not be there, so we remove it manually
    sk_assert(runnable_.size() == 1);
    runnable_.pop();
}

coroutine_mgr::~coroutine_mgr() {
    if (current_) {
        sk_error("coroutine_mgr::~coroutine_mgr() gets called inside a coroutine(%s)!", current_->name);
    }

    if (uv_) {
        sk_warn("uv_ is active!");
    }

    if (!runnable_.empty() || !io_waiting_.empty() || !cond_waiting_.empty() || !sleeping_.empty()) {
        sk_warn("there are still active coroutines!");
    }
}

coroutine *coroutine_mgr::create(const std::string& name,
                                 const coroutine_function& fn,
                                 size_t stack_size, bool preserve_fpu) {
    // TODO: if stack_size is less than 2 pages, adjust it to 2 pages
    coroutine *c = new coroutine(name, fn, stack_size, preserve_fpu);
    if (!c->init()) {
        delete c;
        return nullptr;
    }

    runnable_.push(c);
    return c;
}

coroutine *coroutine_mgr::self() {
    return current_;
}

const char *coroutine_mgr::name() {
    return current_ ? current_->name : nullptr;
}

void coroutine_mgr::sleep(u64 ms) {
    sk_assert(current_ && current_->state == state_running);

    heap_timer *t = new heap_timer(loop_, [this] (heap_timer *t) {
        auto it = sleeping_.find(t);
        assert_ensure(it != sleeping_.end()) {
            coroutine *c = it->second;
            sleeping_.erase(it);

            c->state = state_runnable;
            runnable_.push(c);

            sk_assert(current_ == uv_);
            yield(current_);
        }

        t->close([] (heap_timer *t) {
            delete t;
        });
    });

    t->start_once(ms);
    current_->state = state_sleeping;
    sleeping_.insert(std::make_pair(t, current_));
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

NS_END(detail)
NS_END(sk)
