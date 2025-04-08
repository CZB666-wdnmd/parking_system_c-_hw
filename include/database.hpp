#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <mutex>
#include <fstream>

using json = nlohmann::json;

class Database {
public:
    static Database& getInstance();
    json getUsers();
    bool saveUsers(const json& data);
    json getVehicles();
    bool saveVehicles(const json& data);

private:
    Database() = default;
    std::mutex usersMutex;
    std::mutex vehiclesMutex;

    json readJson(const std::string& filename);
    bool writeJson(const std::string& filename, const json& data);
};