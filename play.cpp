#include <vector>
#include "EDstaff.h"
#include <iostream>

int main() {
    std::vector<std::pair<TimePoint, TimePoint>> shifts;

    // Add two shifts
    shifts.emplace_back(make_time(2025, 8, 1, 8), make_time(2025, 8, 1, 16));
    shifts.emplace_back(make_time(2025, 8, 2, 8), make_time(2025, 8, 2, 16));

    // Iterate like Python list
    for (const auto& [start, end] : shifts) {
        std::cout << "Shift from " << format_time(start)
                  << " to " << format_time(end) << "\n";
    }
}