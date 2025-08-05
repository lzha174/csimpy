//
// Created by ozhang on 03/08/2025.
//

#ifndef SIMSETTINGS_H
#define SIMSETTINGS_H

#include <chrono>
#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>

struct SimSettings {
    using TimePoint = std::chrono::system_clock::time_point;

    TimePoint start_time;

    explicit SimSettings(TimePoint start) : start_time(start) {}

    int minutes_from_start(const TimePoint& to) const {
        return static_cast<int>(std::chrono::duration_cast<std::chrono::minutes>(to - start_time).count());
    }

    std::string minutes_from_start_str(const TimePoint& to) const {
        return std::to_string(minutes_from_start(to)) + " minutes";
    }

    TimePoint current_time(int env_time_minutes) const {
        return start_time + std::chrono::minutes(env_time_minutes);
    }

    static std::string format(const TimePoint& tp) {
        std::time_t t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm{};
#if defined(_MSC_VER) || defined(__MINGW32__)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char buffer[20];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &tm);
        return std::string(buffer);
    }

    std::string current_time_str(int env_time_minutes) const {
        return format(current_time(env_time_minutes));
    }
};

#endif //SIMSETTINGS_H
