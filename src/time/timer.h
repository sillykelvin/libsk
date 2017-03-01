#ifndef TIMER_H
#define TIMER_H

#include "utility/types.h"

#define TICK_PER_SEC 50 // 50 tick per second

namespace sk {
namespace time {

typedef void (*TIMEOUT_CALLBACK)(u64 timer_mid, void *cb_data, size_t cb_len);

inline u32 sec2tick(u32 sec) {
    return sec * TICK_PER_SEC;
}

inline u64 ms2tick(u64 ms) {
    return (ms / 1000) * TICK_PER_SEC;
}

int register_timeout_callback(int timer_type, TIMEOUT_CALLBACK fn_callback);

u64 add_timer(u32 first_interval_sec, u32 repeat_interval_sec,
              int repeat_count, int timer_type,
              const void *cb_data, size_t cb_len);

u64 add_timer(u32 interval_sec, int repeat_count,
              int timer_type, const void *cb_data, size_t cb_len);

u64 add_forever_timer(u32 first_interval_sec, u32 repeat_interval_sec,
                      int timer_type, const void *cb_data, size_t cb_len);

u64 add_forever_timer(u32 interval_sec, int timer_type,
                      const void *cb_data, size_t cb_len);

u64 add_single_timer(u32 interval_sec, int timer_type,
                     const void *cb_data, size_t cb_len);

void remove_timer(u64 timer_mid);

int now();

u64 current_tick();
u64 current_msec();
u64 current_usec();

bool timer_enabled();

int init_timer(int shm_type, int time_offset);

void run_timer();

} // namespace time
} // namespace sk

#endif // TIMER_H
