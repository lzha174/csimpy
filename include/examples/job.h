//
// Created by ozhang on 05/08/2025.
//

#ifndef JOB_H
#define JOB_H

#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>
#include <map>
#include "skill.h"

// Aliases for time types
using TimePoint = std::chrono::system_clock::time_point;
using Minutes   = std::chrono::minutes;
#include <vector>

/// Represents a work job with an arrival time and a duration.
struct Job {
    TimePoint arrive_time;  ///< when the job arrives
    Minutes   duration;     ///< how long the job takes
    std::map<Skill, int> skill_request;  ///< number of staff needed per skill

    Job(const TimePoint& at, Minutes dur)
      : arrive_time(at), duration(dur) {}

    /// Return a human-readable description of the job.
    std::string to_string() const {
        std::ostringstream oss;
        auto tt = std::chrono::system_clock::to_time_t(arrive_time);
        std::tm tm = *std::localtime(&tt);
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M");
        oss << " (duration: " << duration.count() << " min)";
        return oss.str();
    }
};


/// Manages a collection of Job objects.
class JobManager {
public:
    /// Add a new job to the manager.
    void add_job(const Job& job) { jobs_.push_back(job); }
    /// Returns all stored jobs.
    const std::vector<Job>& jobs() const { return jobs_; }
private:
    std::vector<Job> jobs_;  ///< storage for jobs
};

#endif //JOB_H
