#ifndef TIME_HELPER_H
#define TIME_HELPER_H

#include <ctime>

namespace sk {
namespace time {

struct tm *localtime_safe(time_t t);
int day_start_time(time_t t);
bool is_same_day(time_t t1, time_t t2, int offset_hour);
bool is_same_week(time_t t1, time_t t2, int offset_hour);

} // namespace time
} // namespace sk

#endif // TIME_HELPER_H
