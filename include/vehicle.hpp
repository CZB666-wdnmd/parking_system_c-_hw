#pragma once
#include <string>

class VehicleManager {
public:
    static bool entry(const std::string& plate, const std::string& time, std::string& msg);
    static bool exit(const std::string& plate, const std::string& time, double& fee, std::string& duration, std::string& msg);
};