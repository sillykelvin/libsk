#ifndef SHM_TIMER_H
#define SHM_TIMER_H

#include "utility/types.h"

NS_BEGIN(sk)
NS_BEGIN(time)

typedef void (*TIMEOUT_CALLBACK)(u64 timer_mid, void *cb_data, size_t cb_len);

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

bool shm_timer_enabled();
void run_shm_timer();
int init_shm_timer(int shm_type);

NS_END(time)
NS_END(sk)

#endif // SHM_TIMER_H
