#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <stdexcept>
#include <string>

namespace brave_history {

inline constexpr int64_t CHROME_EPOCH_OFFSET_MICROS = 11644473600000000LL;
inline const std::string DEFAULT_TIME_ZONE = "Europe/Dublin";

inline int64_t chromeMicrosToUnixMs(int64_t chromeMicros) {
    return (chromeMicros - CHROME_EPOCH_OFFSET_MICROS) / 1000LL;
}

inline int64_t unixMsToChromeMicros(int64_t unixMs) {
    return unixMs * 1000LL + CHROME_EPOCH_OFFSET_MICROS;
}

namespace detail {

inline std::mutex g_tz_mutex;

} // namespace detail

inline std::mutex& env_mutex() {
    return detail::g_tz_mutex;
}

inline std::string timeZone() {
    std::lock_guard<std::mutex> lock(env_mutex());
    const char* env = std::getenv("BRAVE_HISTORY_TZ");
    return (env && *env) ? std::string(env) : DEFAULT_TIME_ZONE;
}

namespace detail {

struct TzGuard {
    std::string old_tz;
    bool had_old = false;

    explicit TzGuard(const std::string& tz) {
        g_tz_mutex.lock();
        const char* cur = std::getenv("TZ");
        if (cur) {
            old_tz = cur;
            had_old = true;
        }
        setenv("TZ", tz.c_str(), 1);
        tzset();
    }

    ~TzGuard() {
        if (had_old) {
            setenv("TZ", old_tz.c_str(), 1);
        } else {
            unsetenv("TZ");
        }
        tzset();
        g_tz_mutex.unlock();
    }
};

inline int64_t get_gmtoff_ms(int64_t unix_ms, const std::string& tz) {
    TzGuard guard(tz);
    time_t sec = static_cast<time_t>(unix_ms / 1000);
    struct tm tm_out{};
    localtime_r(&sec, &tm_out);
    return static_cast<int64_t>(tm_out.tm_gmtoff) * 1000LL;
}

} // namespace detail

inline std::string format_local_iso(int64_t unix_ms, const std::string& tz) {
    detail::TzGuard guard(tz);
    time_t sec = static_cast<time_t>(unix_ms / 1000);
    struct tm tm_out{};
    localtime_r(&sec, &tm_out);
    int offset_sec = static_cast<int>(tm_out.tm_gmtoff);
    char sign = offset_sec < 0 ? '-' : '+';
    int abs_off = std::abs(offset_sec);
    int off_h = abs_off / 3600;
    int off_m = (abs_off % 3600) / 60;

    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
             tm_out.tm_year + 1900,
             tm_out.tm_mon + 1,
             tm_out.tm_mday,
             tm_out.tm_hour,
             tm_out.tm_min,
             tm_out.tm_sec,
             sign, off_h, off_m);
    return std::string(buf);
}

inline int64_t local_day_start_ms(const std::string& date, const std::string& tz) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') {
        throw std::runtime_error("Invalid date (expected YYYY-MM-DD): " + date);
    }
    for (size_t i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(date[i]))) {
            throw std::runtime_error("Invalid date (expected YYYY-MM-DD): " + date);
        }
    }
    int y = std::stoi(date.substr(0, 4));
    int m = std::stoi(date.substr(5, 2));
    int d = std::stoi(date.substr(8, 2));

    struct tm tm_utc{};
    tm_utc.tm_year = y - 1900;
    tm_utc.tm_mon = m - 1;
    tm_utc.tm_mday = d;
    tm_utc.tm_hour = 0;
    tm_utc.tm_min = 0;
    tm_utc.tm_sec = 0;
    time_t guess_sec = timegm(&tm_utc);
    int64_t guess_ms = static_cast<int64_t>(guess_sec) * 1000LL;

    int64_t first = guess_ms - detail::get_gmtoff_ms(guess_ms, tz);
    int64_t second = guess_ms - detail::get_gmtoff_ms(first, tz);
    return second;
}

inline std::string nextDay(const std::string& date) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') {
        throw std::runtime_error("Invalid date (expected YYYY-MM-DD): " + date);
    }
    int y = std::stoi(date.substr(0, 4));
    int m = std::stoi(date.substr(5, 2));
    int d = std::stoi(date.substr(8, 2));

    struct tm tm_utc{};
    tm_utc.tm_year = y - 1900;
    tm_utc.tm_mon = m - 1;
    tm_utc.tm_mday = d + 1;
    tm_utc.tm_hour = 0;
    tm_utc.tm_min = 0;
    tm_utc.tm_sec = 0;
    time_t next_sec = timegm(&tm_utc);
    struct tm tm_res{};
    gmtime_r(&next_sec, &tm_res);
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm_res.tm_year + 1900, tm_res.tm_mon + 1, tm_res.tm_mday);
    return std::string(buf);
}

} // namespace brave_history
