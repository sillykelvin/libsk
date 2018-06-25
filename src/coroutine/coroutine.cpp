#include <queue>
#include <log/log.h>
#include <unordered_set>
#include <time/heap_timer.h>
#include <coroutine/coroutine.h>
#include <utility/assert_helper.h>
#include <coroutine/detail/context.h>

NS_BEGIN(sk)

struct coroutine_mgr;
static void *main_ctx = nullptr;
static coroutine_mgr *mgr = nullptr;

struct coroutine_mgr {
    uv_loop_t *loop_;
    coroutine_ptr uv_;
    coroutine_ptr current_;
    std::queue<coroutine_ptr> runnable_;
    std::unordered_set<coroutine_ptr> io_waiting_;
    std::unordered_set<coroutine_ptr> cond_waiting_;
    std::unordered_map<heap_timer*, coroutine_ptr> sleeping_;

    coroutine_mgr(uv_loop_t *loop) : loop_(loop) {}
    ~coroutine_mgr() {
        // TODO
    }

    void add_runnable(const coroutine_ptr& ptr) {
        sk_assert(ptr->runnable());
        runnable_.push(ptr);
    }

    void add_io_waiting(const coroutine_ptr& ptr) {
        sk_assert(ptr->io_waiting());
        io_waiting_.insert(ptr);
    }

    void add_cond_waiting(const coroutine_ptr& ptr) {
        sk_assert(ptr->cond_waiting());
        cond_waiting_.insert(ptr);
    }

    void add_sleeping(const coroutine_ptr& ptr, heap_timer *t) {
        sk_assert(ptr->sleeping());
        sleeping_.insert(std::make_pair(t, ptr));
    }
};

coroutine::~coroutine() {
    sk_trace("coroutine::~coroutine(%p), state(%d)", this, state_);

    if (state_ != state_finished)
        sk_warn("coroutine(%p:%d) is not done!", this, state_);

    if (stack_) {
        free(stack_);
        stack_ = nullptr;
        stack_size_ = 0;

        // ctx_ actually points to an object on stack_, so reset it
        // after stack_ get freed
        ctx_ = nullptr;
    }
}

coroutine_ptr coroutine::create(const coroutine_function& fn,
                                size_t stack_size, bool preserve_fpu) {
    coroutine *c = new coroutine();

    c->fn_ = fn;
    c->flag_ = 0;
    c->state_ = state_runnable;
    c->stack_size_ = stack_size;

    c->stack_ = char_ptr(malloc(c->stack_size_));
    if (!c->stack_) {
        delete c;
        return nullptr;
    }

    // stack grows from top to bottom
    c->ctx_ = sk::detail::make_context(c->stack_ + c->stack_size_, c->stack_size_, context_main);
    if (!c->ctx_) {
        delete c;
        return nullptr;
    }

    if (preserve_fpu) {
        c->flag_ |= FLAG_PRESERVE_FPU;
    }

    sk_trace("coroutine::create(%p)", c);
    coroutine_ptr ptr = coroutine_ptr(c);
    mgr->add_runnable(ptr);
    return ptr;
}

coroutine_ptr coroutine::current_coroutine() {
    return mgr->current_;
}

void coroutine::schedule() {
    for (;;) {
        if (mgr->runnable_.empty()) {
            mgr->uv_->set_running();
            mgr->current_ = mgr->uv_;
            mgr->current_->resume();
            mgr->current_ = nullptr;

            if (mgr->uv_->finished()) {
                break;
            }

            continue;
        }

        coroutine_ptr ptr = mgr->runnable_.front();
        mgr->runnable_.pop();

        ptr->set_running();
        mgr->current_ = ptr;
        ptr->resume();
        mgr->current_ = nullptr;

        if (ptr->finished()) {
            sk_trace("coroutine(%p) finised.", ptr.get());
        }
    }
}

void coroutine::sleep(u64 ms) {
    sk_assert(running());

    heap_timer *t = new heap_timer(mgr->loop_, on_sleep_timeout);
    t->start_once(ms);

    state_ = state_sleeping;
    mgr->add_sleeping(shared_from_this(), t);
    yield();
}

void coroutine::yield() {
    sk::detail::jump_context(&ctx_, main_ctx, reinterpret_cast<intptr_t>(this), (flag_ & FLAG_PRESERVE_FPU) != 0);
}

void coroutine::resume() {
    sk::detail::jump_context(&main_ctx, ctx_, reinterpret_cast<intptr_t>(this), (flag_ & FLAG_PRESERVE_FPU) != 0);
}

void coroutine::context_main(intptr_t arg) {
    coroutine *c = reinterpret_cast<coroutine*>(arg);
    c->fn_();

    c->state_ = state_finished;
    c->yield();
}

void coroutine::on_sleep_timeout(heap_timer *t) {
    auto it = mgr->sleeping_.find(t);
    assert_ensure(it != mgr->sleeping_.end()) {
        coroutine_ptr ptr = it->second;
        mgr->sleeping_.erase(it);

        ptr->set_runnable();
        mgr->add_runnable(ptr);

        sk_assert(mgr->current_ == mgr->uv_);
        mgr->current_->yield();
    }

    t->close([] (heap_timer *t) { delete t; });
}

void coroutine_init(uv_loop_t *loop) {
    if (!mgr) {
        mgr = new coroutine_mgr(loop);

        mgr->uv_ = coroutine::create([] () {
            uv_run(mgr->loop_, UV_RUN_DEFAULT);
        });

        // remove the uv coroutine from the runnable
        // queue, which is added by coroutine::create(...)
        sk_assert(mgr->runnable_.size() == 1);
        mgr->runnable_.pop();
    }
}

void coroutine_fini() {
    if (mgr) {
        delete mgr;
        mgr = nullptr;
    }
}

void coroutine_schedule() {
    coroutine::schedule();
}

NS_END(sk)
