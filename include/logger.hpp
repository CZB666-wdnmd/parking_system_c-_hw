#pragma once
#include <string>
#include <mutex>
#include <nlohmann/json.hpp>

class Logger {
public:
    static void logUser(const std::string& username, const std::string& action, const std::string& message);
    static void logVehicle(const std::string& plate, const std::string& action, const std::string& message);
    static void logAdmin(const std::string& admin, const std::string& action, const std::string& plate, const std::string& message);

private:
    static std::mutex logMutex;
    static void writeLog(const nlohmann::json& log);
};