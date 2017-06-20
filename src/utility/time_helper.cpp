#include "time_helper.h"
#include "log/log.h"
#include "utility/assert_helper.h"

#define SECONDS_OF_DAY (24 * 3600)

NS_BEGIN(sk)
NS_BEGIN(time)

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

bool is_same_week(time_t t1, time_t t2, int offset_hour) {
    assert_retval(offset_hour >= 0 && offset_hour <= 23, false);

    tm tm_value1 = *localtime_safe(t1 - offset_hour * 60 * 60);
    tm tm_value2 = *localtime_safe(t2 - offset_hour * 60 * 60);

    // the original tm_wday starts from SUNDAY:
    //   SUNDAY(0) -> MONDAY(1) -> ... -> SATURDAY(6)
    // but we consider week starts from MONDAY:
    //   MONDAY(0) -> TUESDAY(1) -> ... -> SUNDAY(6)
    // the following method can do the transformation
    int weekday1 = (tm_value1.tm_wday + 7 - 1) % 7;
    int weekday2 = (tm_value2.tm_wday + 7 - 1) % 7;

    // set it to day begining time
    tm_value1.tm_hour = 0;
    tm_value1.tm_min  = 0;
    tm_value1.tm_sec  = 0;
    tm_value2.tm_hour = 0;
    tm_value2.tm_min  = 0;
    tm_value2.tm_sec  = 0;

    time_t week1 = mktime(&tm_value1) - (weekday1 * SECONDS_OF_DAY);
    time_t week2 = mktime(&tm_value2) - (weekday2 * SECONDS_OF_DAY);
    return week1 == week2;
}

void timeval_add(const timeval& tv1, const timeval& tv2, timeval& out) {
    out.tv_sec = tv1.tv_sec + tv2.tv_sec;
    out.tv_usec = tv1.tv_usec + tv2.tv_usec;
    if (out.tv_usec >= 1000000) {
        out.tv_sec += 1;
        out.tv_usec -= 1000000;
    }
}

void timeval_sub(const timeval& tv1, const timeval& tv2, timeval& out) {
    out.tv_sec = tv1.tv_sec - tv2.tv_sec;
    out.tv_usec = tv1.tv_usec - tv2.tv_usec;
    if (out.tv_usec < 0) {
        out.tv_sec -= 1;
        out.tv_usec += 1000000;
    }
}

NS_END(time)
NS_END(sk)
