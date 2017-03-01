#include "time_helper.h"
#include "log/log.h"
#include "utility/assert_helper.h"

#define SECONDS_OF_DAY (24 * 3600)

namespace sk {
namespace time {

struct tm *localtime_safe(time_t t) {
    struct tm *tm = localtime(&t);
    if (tm) return tm;

    sk_fatal("invalid time<%ld>.", t);

    t = std::time(NULL);
    t -= 3 * SECONDS_OF_DAY; // 3 days ago

    return localtime(&t);
}

int day_start_time(time_t t) {
    struct tm tm;
    memset(&tm, 0x00, sizeof(tm));

    tm = *localtime_safe(t);
    tm.tm_sec  = 0;
    tm.tm_min  = 0;
    tm.tm_hour = 0;

    return static_cast<int>(mktime(&tm));
}

bool is_same_day(time_t t1, time_t t2, int offset_hour) {
    assert_retval(offset_hour >= 0 && offset_hour <= 23, false);

    tm tm_value1 = *localtime_safe(t1 - offset_hour * 60 * 60);
    tm tm_value2 = *localtime_safe(t2 - offset_hour * 60 * 60);

    if (tm_value1.tm_mday != tm_value2.tm_mday)
        return false;

    if (tm_value1.tm_mon != tm_value2.tm_mon)
        return false;

    if (tm_value1.tm_year != tm_value2.tm_year)
        return false;

    return true;
}

} // namespace time
} // namespace sk
