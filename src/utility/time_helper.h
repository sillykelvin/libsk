#ifndef TIME_HELPER_H
#define TIME_HELPER_H

#include <ctime>
#include <utility/types.h>

NS_BEGIN(sk)
NS_BEGIN(time)

struct tm *localtime_safe(time_t t);
int day_start_time(time_t t);
bool is_same_day(time_t t1, time_t t2, int offset_hour);
bool is_same_week(time_t t1, time_t t2, int offset_hour);

void timeval_add(const timeval& tv1, const timeval& tv2, timeval *out);
void timeval_sub(const timeval& tv1, const timeval& tv2, timeval *out);

NS_END(time)
NS_END(sk)

#endif // TIME_HELPER_H
