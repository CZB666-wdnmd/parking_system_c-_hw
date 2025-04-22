#pragma once
#include <string>

class VehicleManager {
public:
    static bool entry(const std::string& plate, const std::string& time, std::string& msg);
    static bool exit(const std::string& plate, const std::string& time, double& fee, std::string& duration, std::string& msg);
    static bool addMonthly(const std::string& plate, int days, std::string& msg);
    static bool addBlacklist(const std::string& plate, std::string& msg);
    static bool removeBlacklist(const std::string& plate, std::string& msg);
    static bool getDuration(const std::string& plate, const std::string& time, std::string& duration, double& fee, std::string& msg);
};