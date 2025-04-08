#include "../include/vehicle.hpp"
#include "../include/database.hpp"
#include "../include/utils.hpp"
#include <sstream>


bool VehicleManager::entry(const std::string& plate, const std::string& time, std::string& msg) {
    auto& db = Database::getInstance();
    auto vehicles = db.getVehicles();

    if (vehicles.contains(plate)) {
        auto& v = vehicles[plate];
        if (v["is_inside"] == true) {
            msg = "Vehicle already inside";
            return false;
        }
        if (v["is_blacklisted"] == true) {
            msg = "Blacklisted vehicle";
            return false;
        }
        v["is_inside"] = true;
        v["entry_time"] = time;
        v["history_entries"].push_back(time);
    } else {
        vehicles[plate] = {
            {"license_plate", plate},
            {"is_inside", true},
            {"is_monthly", false},
            {"is_blacklisted", false},
            {"entry_time", time},
            {"history_entries", {time}},
            {"history_exits", json::array()}
        };
    }

    if (db.saveVehicles(vehicles)) {
        msg = "Entry successful";
        return true;
    }
    msg = "Database error";
    return false;
}

bool VehicleManager::exit(const std::string& plate, const std::string& time, double& fee, std::string& duration, std::string& msg) {
    auto& db = Database::getInstance();
    auto vehicles = db.getVehicles();

    if (!vehicles.contains(plate)) {
        msg = "Vehicle not found";
        return false;
    }
    auto& v = vehicles[plate];
    if (v["is_inside"] != true) {
        msg = "Vehicle not inside";
        return false;
    }

    //从config.json中读取计费配置
    std::ifstream config_file("config.json");
    json config;
    int freeTime = config["freetime"];
    int stageTime = config["fee_stage_time"];
    double stagePrice = config["fee_stage_price"];
    double dayTop = config["fee_day_top"];

    //计算费用
    double totalHours = utils::calculateHours(v["entry_time"], time);
    int totalSec = static_cast<int>(totalHours * 3600 + 0.5);
    int totalMin = (totalSec + 59) / 60;

    int chargeableMinutes = (totalMin > freeTime) ? (totalMin - freeTime) : 0;
    int days = chargeableMinutes / (24 * 60);
    int remainder = chargeableMinutes % (24 * 60);
    int stages = (remainder + stageTime - 1) / stageTime;
    fee = days * dayTop + stages * stagePrice;

    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << h << ":"
        << std::setw(2) << std::setfill('0') << m << ":"
        << std::setw(2) << std::setfill('0') << s;
    duration = oss.str();

    v["is_inside"] = false;
    v["history_exits"].push_back(time);
    if (db.saveVehicles(vehicles)) {
        msg = "Exit successful";
        return true;
    }
    msg = "Database error";
    return false;
}