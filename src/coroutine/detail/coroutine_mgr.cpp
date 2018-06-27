#include <sys/mman.h>
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

NS_BEGIN(detail)

static void *main_ctx = nullptr;

coroutine_mgr::coroutine_mgr(uv_loop_t *loop) : loop_(loop), uv_(nullptr), current_(nullptr) {
    uv_ = create("uv", [this] () {
        uv_run(loop_, UV_RUN_DEFAULT);
    }, 16 * 1024, false, true);

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
