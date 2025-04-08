#include "../include/logger.hpp"
#include "../include/utils.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>

std::mutex Logger::logMutex;

void Logger::writeLog(const nlohmann::json& log) {
    std::lock_guard<std::mutex> lock(logMutex);
    // print log to terminal in human-readable format
    std::cout << log.dump(4) << std::endl;
    std::ofstream file("system.log", std::ios::app);
    if (file) {
        file << log.dump() << "\n";
    }
}

void Logger::logUser(const std::string& username, const std::string& action, const std::string& message) {
    nlohmann::json log = {
        {"timestamp", utils::getCurrentTimeISO()},
        {"user", username},
        {"action", action},
        {"message", message}
    };
    writeLog(log);
}

void Logger::logVehicle(const std::string& plate, const std::string& action, const std::string& message) {
    nlohmann::json log = {
        {"timestamp", utils::getCurrentTimeISO()},
        {"license_plate", plate},
        {"action", action},
        {"message", message}
    };
    writeLog(log);
}

void Logger::logAdmin(const std::string& admin, const std::string& action, const std::string& plate, const std::string& message) {
    nlohmann::json log = {
        {"timestamp", utils::getCurrentTimeISO()},
        {"user", admin},
        {"action", action},
        {"license_plate", plate},
        {"message", message}
    };
    writeLog(log);
}