#ifndef TIME_H
#define TIME_H

#include "utility/types.h"

NS_BEGIN(sk)
NS_BEGIN(time)

/*
 * timers with value not a multiple of 20ms will be
 * rounded to a multiple of 20ms, e.g. 17ms will be
 * 0ms, 105ms will be 100ms
 */
static const u64 TIMER_PRECISION = 20; // 20 ms

/*
 * return the number of seconds since the Epoch,
 * which represents current process time, may differ
 * from real clock time as a time offset might be
 * specified during process start
 */
int now();

/*
 * returns true if time manager is enabled
 * if in_shm is not nullptr, store whether
 * the time manager is in shm or not in it
 */
bool time_enabled(bool *in_shm);

/*
 * returns tick count of interval represented
 * by sec (second)
 */
u64 sec2tick(u32 sec);

/*
 * return current tick count of process time
 */
u64 current_tick();

/*
 * setup process time management, this will put the
 * internal time manager into shared memory, it's a
 * MUST if the shm timers are used
 * shm_type is the singleton shm type to store the time
 * manager, time_offset is the offset between process
 * time and real clock time, in second
 * returns 0 if succeeded, error code otherwise
 */
int init_time(int shm_type, int time_offset);

/*
 * setup process time management, the difference
 * between this and the above is that this will
 * only put the internal time manager into normal
 * memory, so shm timers cannot be used, this can
 * be used if the process does not need shm timers
 */
int init_time(int time_offset);

/*
 * make internal time manager to update process time
 * and also run those overtime timers
 * NOTE: this function should be called periodically
 * however, the calling interval should be proper:
 * 1. if the interval is too big, the time retrieved
 *    by now() would be not accurate, and the timers
 *    will also be affected
 * 2. if the interval is too small, the performance
 *    will be bad
 */
void update_time();

NS_END(time)
NS_END(sk)

#endif // TIME_H
