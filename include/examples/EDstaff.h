//
// Created by ozhang on 03/08/2025.
//

#ifndef EDSTAFF_H
#define EDSTAFF_H
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include "../csimpy/itembase.h"
#include "skill.h"

// Aliases to mirror “datetime”
using TimePoint = std::chrono::system_clock::time_point;
using Hours = std::chrono::hours;
using Minutes = std::chrono::minutes;
using Days = std::chrono::days;

inline std::string format_time(const TimePoint& tp) {
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&tt);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M");
    return oss.str();
}

// Construct a TimePoint like Python's datetime(...)
inline TimePoint make_time(int year, int month, int day, int hour = 0, int minute = 0) {
    std::tm tm = {};
    tm.tm_year = year - 1900;  // years since 1900
    tm.tm_mon = month - 1;     // months since January
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = 0;
    std::time_t tt = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(tt);
}

// Shift and EDStaff definitions
struct Shift {
    TimePoint start;
    TimePoint end;
    Shift(const TimePoint& s, const TimePoint& e) : start(s), end(e) {}
    bool overlaps(const Shift& other) const {
        return start < other.end && end > other.start;
    }
    std::string to_string() const {
        return "[" + format_time(start) + " - " + format_time(end) + "]";
    }
};

class EDStaff : public ItemBase {
public:
    std::vector<Shift> shifts;
    std::vector<Shift> breaks;
    Skill skill;  // staff skill level

    EDStaff(const std::string& name_, int id_, Skill level_ = Skill::Junior)
        : ItemBase(name_, id_), skill(level_) {}

    // Add a shift, reject if overlaps any existing shift
    bool add_shift(const Shift& s) {
        for (const auto& sh : shifts) {
            if (sh.overlaps(s)) return false;
        }
        shifts.push_back(s);
        return true;
    }

    // Add multiple shifts, reject if any fail
    bool add_shifts(const std::vector<Shift>& list) {
        for (const auto& s : list) {
            if (!add_shift(s)) return false;
        }
        return true;
    }

    // Get shift at a time point, or nullptr if none
    const Shift* get_shift_at(const TimePoint& tp) const {
        for (const auto& sh : shifts) {
            if (tp >= sh.start && tp < sh.end) return &sh;
        }
        return nullptr;
    }

    // Derive all breaks (gaps) between shifts, starting from a reference time
    std::vector<Shift> derive_breaks(const TimePoint& start_time) const {
        std::vector<Shift> sorted = shifts;
        std::sort(sorted.begin(), sorted.end(), [](const Shift& a, const Shift& b) { return a.start < b.start; });
        std::vector<Shift> breaks;
        if (sorted.empty()) return breaks;
        // first break: from provided start_time to first shift start
        if (start_time < sorted[0].start) {
            breaks.emplace_back(start_time, sorted[0].start);
        }
        // gaps between consecutive shifts
        for (size_t i = 1; i < sorted.size(); ++i) {
            if (sorted[i-1].end < sorted[i].start) {
                breaks.emplace_back(sorted[i-1].end, sorted[i].start);
            }
        }
        return breaks;
    }

    // Update breaks based on current shifts and a given start_time
    void update_breaks(const TimePoint& start_time) {
        breaks = derive_breaks(start_time);
    }

    // Getter for breaks
    const std::vector<Shift>& get_breaks() const { return breaks; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "EDStaff(name=" << name << ", id=" << id << ", shifts=[";
        for (size_t i = 0; i < shifts.size(); ++i) {
            oss << shifts[i].to_string();
            if (i + 1 < shifts.size()) oss << ", ";
        }
        oss << "]";
        // Append breaks info
        oss << ", breaks=[";
        for (size_t i = 0; i < breaks.size(); ++i) {
            oss << breaks[i].to_string();
            if (i + 1 < breaks.size()) oss << ", ";
        }
        oss << "])";
        return oss.str();
    }

    ItemBase* clone() const override { return new EDStaff(*this); }
};


#endif //EDSTAFF_H
