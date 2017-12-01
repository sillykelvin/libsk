#ifndef SHM_TIMER_H
#define SHM_TIMER_H

#include <uv.h>
#include <shm/shm_ptr.h>
#include <utility/singleton.h>

NS_BEGIN(sk)
NS_BEGIN(time)

class shm_timer;
using shm_timer_ptr = shm_ptr<shm_timer>;

class shm_timer {
public:
    MAKE_NONCOPYABLE(shm_timer);

    template<typename T>
    static shm_timer_ptr start_once(s64 timeout_ms,
                                    int timer_type, const T& t) {
        // as we uses memcpy() in start() for the callback data "t",
        // so the callback data "t" MUST be a POD type here
        static_assert(std::is_pod<T>::value, "T must be a POD type.");
        return start(timeout_ms, 0, 1, timer_type, &t, sizeof(t));
    }

    template<typename T>
    static shm_timer_ptr start_forever(s64 timeout_ms, s64 repeat_ms,
                                       int timer_type, const T& t) {
        // as we uses memcpy() in start() for the callback data "t",
        // so the callback data "t" MUST be a POD type here
        static_assert(std::is_pod<T>::value, "T must be a POD type.");
        return start(timeout_ms, repeat_ms, REPEAT_FOREVER, timer_type, &t, sizeof(t));
    }

    template<typename T>
    static shm_timer_ptr start_specified(s64 timeout_ms, s64 repeat_ms,
                                         int repeat_count, int timer_type, const T& t) {
        // as we uses memcpy() in start() for the callback data "t",
        // so the callback data "t" MUST be a POD type here
        static_assert(std::is_pod<T>::value, "T must be a POD type.");
        return start(timeout_ms, repeat_ms, repeat_count, timer_type, &t, sizeof(t));
    }

    int type() const { return timer_type_; }
    s64 expires() const { return expires_; }
    bool running() const { return running_; }
    bool repeats() const { return repeats_ > 0 || repeats_ == REPEAT_FOREVER; }

    /*
     * this function will stop the timer and free the memory, do
     * NOT use this timer anymore after this function gets called
     */
    void stop();

private:
    shm_timer()
        : self_(nullptr), prev_(nullptr), next_(nullptr),
          expires_(INT64_MAX), interval_(INT64_MAX),
          running_(false), repeats_(0), timer_type_(-1), cb_len_(0) {}

    // we do NOT allow the user call shm_delete(timer_ptr), as the timer will
    // be released in shm_timer_mgr, so we make the destructor private here
    ~shm_timer() { sk_assert(!prev_ && !next_); }

private:
    // NOTE: there is no "const" here, as some callbacks
    // need to modify the callback data outside the timer
    void *data() { return cb_data_; }
    size_t data_length() const { return cb_len_; }

    const shm_timer_ptr& self() const { return self_; }
    const shm_timer_ptr& prev() const { return prev_; }
    const shm_timer_ptr& next() const { return next_; }

    void set_self_ptr(const shm_timer_ptr& ptr) { self_ = ptr; }
    void set_prev_ptr(const shm_timer_ptr& ptr) { prev_ = ptr; }
    void set_next_ptr(const shm_timer_ptr& ptr) { next_ = ptr; }

    void running_on() { running_ = true; }
    void running_off() { running_ = false; }
    void refresh(s64 current_tick) {
        if (repeats_ > 0) repeats_ -= 1;
        if (repeats_ > 0 || repeats_ == REPEAT_FOREVER)
            expires_ = current_tick + interval_;
    }

private:
    static shm_timer_ptr start(s64 timeout_ms, s64 repeat_ms, int repeat_count,
                               int timer_type, const void *data, size_t len);

    static shm_timer_ptr construct(size_t extra_len);
    static void destruct(shm_timer_ptr ptr);

private:
    static const int REPEAT_FOREVER = -1;

    shm_timer_ptr self_; // this timer
    shm_timer_ptr prev_; // prev timer
    shm_timer_ptr next_; // next timer
    s64 expires_;        // tick count till next timeout
    s64 interval_;       // timeout interval, in tick
    bool running_;       // is in timeout callback or not
    int repeats_;        // repeat count, might be REPEAT_FOREVER
    int timer_type_;     // callback id
    size_t cb_len_;      // callback data length
    char cb_data_[0];    // callback data

    template<size_t SIZE>
    friend class timer_vector;
    friend class timer_helper;
    friend class shm_timer_mgr;
};

class shm_timer_callback_base {
public:
    virtual ~shm_timer_callback_base() = default;
    virtual void on_timeout(shm_timer_ptr timer, void *data, size_t len) = 0;
};

template<typename T>
class shm_timer_callback : public shm_timer_callback_base {
public:
    using fn_callback = void(*)(shm_timer_ptr timer, T *t);

    explicit shm_timer_callback(fn_callback fn) : fn_(fn) { sk_assert(fn_); }
    virtual ~shm_timer_callback() = default;

    virtual void on_timeout(shm_timer_ptr timer, void *data, size_t len) {
        assert_retnone((data && len == sizeof(T)) || (!data && len == 0));
        T *t = data ? static_cast<T*>(data) : nullptr;
        return fn_(timer, t);
    }

private:
    fn_callback fn_;
};

class shm_timer_dispatcher {
DECLARE_SINGLETON(shm_timer_dispatcher);
public:
    template<typename T>
    void register_callback(int timer_type, typename shm_timer_callback<T>::fn_callback fn) {
        auto it = type2callbacks_.find(timer_type);
        assert_retnone(it == type2callbacks_.end());

        auto cb = new shm_timer_callback<T>(fn);
        assert_retnone(cb);

        type2callbacks_[timer_type] = callback_ptr(cb);
    }

    bool has_callback(int timer_type) const {
        return type2callbacks_.find(timer_type) != type2callbacks_.end();
    }

    void on_timeout(shm_timer_ptr timer, void *data, size_t len) {
        auto it = type2callbacks_.find(timer->type());
        assert_retnone(it != type2callbacks_.end());

        return it->second->on_timeout(timer, data, len);
    }

private:
    shm_timer_dispatcher() = default;

private:
    using callback_ptr = std::unique_ptr<shm_timer_callback_base>;
    std::unordered_map<int, callback_ptr> type2callbacks_;
};

/*
 * setup the process time management system, this will
 * put the internal timer manager into shared memory
 * time_offset_sec is the offset between process time
 * and real clock time, in second
 * returns 0 if succeeded, error code otherwise
 */
int init(uv_loop_t *loop, int time_offset_sec);

/*
 * stop the process time management system
 */
void stop();

/*
 * destroy the process time management system
 */
void fini();

/*
 * return the number of seconds since the Epoch,
 * which represents current process time, may differ
 * from real clock time as a time offset might be
 * specified during process start
 */
int now();

NS_END(time)
NS_END(sk)

#endif // SHM_TIMER_H
