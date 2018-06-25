#ifndef COROUTINE_H
#define COROUTINE_H

#include <uv.h>
#include <memory>
#include <utility/types.h>

NS_BEGIN(sk)

class coroutine;
class heap_timer;
class condition_variable;
using coroutine_ptr = std::shared_ptr<coroutine>;
using coroutine_function = std::function<void()>;

class coroutine : public std::enable_shared_from_this<coroutine> {
public:
    MAKE_NONCOPYABLE(coroutine);

    static coroutine_ptr create(const coroutine_function& fn,
                                size_t stack_size = DEFAULT_STACK_SIZE,
                                bool preserve_fpu = false);
    ~coroutine();

    static coroutine_ptr current_coroutine();
    static void schedule();

    bool running()      const { return state_ == state_running; }
    bool runnable()     const { return state_ == state_runnable; }
    bool sleeping()     const { return state_ == state_sleeping; }
    bool io_waiting()   const { return state_ == state_io_waiting; }
    bool cond_waiting() const { return state_ == state_cond_waiting; }
    bool finished()     const { return state_ == state_finished; }

    void sleep();       // sleep forever
    void sleep(s64 ms); // sleep for some time

    void wait(condition_variable *cond);                 // wait forever
    void wait(condition_variable *cond, s64 timeout_ms); // wait for some time

private:
    coroutine() = default;

    void yield();
    void resume();

    void set_running() { state_ = state_running; }
    void set_runnable() { state_ = state_runnable; }

    static void context_main(intptr_t arg);
    static void on_sleep_timeout(heap_timer *t);

private:
    static const size_t DEFAULT_STACK_SIZE = 8 * 1024;

    static const int FLAG_PRESERVE_FPU  = 0x1;
    static const int FLAG_TIMEOUT       = 0x2;

    enum state {
        state_running,
        state_runnable,
        state_sleeping,
        state_io_waiting,
        state_cond_waiting,
        state_finished
    };

    int flag_;
    state state_;
    void *ctx_;
    char *stack_;
    size_t stack_size_;
    coroutine_function fn_;
};

void coroutine_init(uv_loop_t *loop);
void coroutine_fini();

void coroutine_schedule();

NS_END(sk)

#endif // COROUTINE_H
