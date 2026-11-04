#include "clock.h"

#include <cstdio>
#include <ctime>

#include "RTC.h"

namespace sims2wide::clock
{
namespace
{
bool fixed = false;
Time fixedTime {};
Stats stats;

// Days since 2000-03-01 (a Howard Hinnant civil-date count, shifted), so that the RTC's
// 2000-2099 range needs no time_t and no zone.
long DaysFromCivil(int y, int m, int d)
{
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}

void CivilFromDays(long z, int& y, int& m, int& d)
{
    z += 719468;
    const long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = (int)yoe + (int)era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = (int)(doy - (153 * mp + 2) / 5 + 1);
    m = (int)(mp < 10 ? mp + 3 : mp - 9);
    y += m <= 2;
}

const long kEpoch = DaysFromCivil(2000, 1, 1);

void Write(melonDS::NDS& nds, const Time& t)
{
    nds.RTC.SetDateTime(t.year, t.month, t.day, t.hour, t.minute, t.second);
}
}

long Time::Seconds() const
{
    return (DaysFromCivil(year, month, day) - kEpoch) * 86400L + hour * 3600L + minute * 60L + second;
}

Time Time::FromSeconds(long s)
{
    Time t {};
    long days = s / 86400, rest = s % 86400;
    if (rest < 0) { rest += 86400; days--; }
    CivilFromDays(days + kEpoch, t.year, t.month, t.day);
    t.hour = (int)(rest / 3600);
    t.minute = (int)(rest / 60 % 60);
    t.second = (int)(rest % 60);
    return t;
}

Time HostNow()
{
    std::time_t now = std::time(nullptr);
    std::tm tm {};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    return { tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec };
}

bool SetFixed(const std::string& text)
{
    Time t {};
    if (sscanf(text.c_str(), "%d-%d-%d %d:%d", &t.year, &t.month, &t.day, &t.hour, &t.minute) != 5) return false;
    if (t.year < 2000 || t.year > 2099 || t.month < 1 || t.month > 12 || t.day < 1 || t.day > 31 ||
        t.hour < 0 || t.hour > 23 || t.minute < 0 || t.minute > 59) return false;
    fixed = true;
    fixedTime = t;
    return true;
}

void Seed(melonDS::NDS& nds)
{
    Write(nds, fixed ? fixedTime : HostNow());
}

void Frame(melonDS::NDS& nds, bool fast)
{
    if (fixed) return;
    const long now = HostNow().Seconds();
    const long rtc = Read(nds).Seconds();
    if (rtc - now > stats.maxAhead) stats.maxAhead = rtc - now;
    if (rtc / 60 > now / 60) stats.minuteAhead++;
    if (fast)
    {
        // melonDS ticks the RTC on emulated time, so fast forward would run it ahead of the
        // world. Hold it: before each frame (one tick at most), set it to one second before the
        // host, or to the host's second at the start of a minute, so that it never shows a
        // minute the host has not reached, and never a minute it already showed.
        long target = now % 60 == 0 ? now : now - 1;
        if (rtc > target) { Write(nds, Time::FromSeconds(target)); stats.writes++; }
    }
    else if (rtc < now - 2)
    {
        // Behind (a pause, the machine asleep, a slow frame): forward is always safe.
        Write(nds, Time::FromSeconds(now));
        stats.writes++;
    }
}

const Stats& GetStats() { return stats; }

Time Read(const melonDS::NDS& nds)
{
    Time t {};
    nds.RTC.GetDateTime(t.year, t.month, t.day, t.hour, t.minute, t.second);
    return t;
}
}
