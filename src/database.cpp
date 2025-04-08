#include "../include/database.hpp"
#include <filesystem>

Database& Database::getInstance() {
    static Database instance;
    return instance;
}

json Database::readJson(const std::string& filename) {
    std::ifstream file(filename);
    if (file.good()) {
        try {
            return json::parse(file);
        } catch (...) {
            if (filename == "users.json") return json::array();
            else return json::object();
        }
    }
    if (filename == "users.json") return json::array();
    else return json::object();
}

bool Database::writeJson(const std::string& filename, const json& data) {
    std::ofstream file(filename);
    if (!file) return false;
    file << data.dump(4);
    return true;
}

json Database::getUsers() {
    std::lock_guard<std::mutex> lock(usersMutex);
    return readJson("users.json");
}

bool Database::saveUsers(const json& data) {
    std::lock_guard<std::mutex> lock(usersMutex);
    return writeJson("users.json", data);
}

json Database::getVehicles() {
    std::lock_guard<std::mutex> lock(vehiclesMutex);
    return readJson("vehicles.json");
}

bool Database::saveVehicles(const json& data) {
    std::lock_guard<std::mutex> lock(vehiclesMutex);
    return writeJson("vehicles.json", data);
}