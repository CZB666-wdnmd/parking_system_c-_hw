#pragma once
#include <string>
#include <ctime>

namespace utils {
    std::string sha256(const std::string& input);
    std::string getCurrentTimeISO();
    time_t isoStringToTime(const std::string& iso);
    double calculateHours(const std::string& start, const std::string& end);
}