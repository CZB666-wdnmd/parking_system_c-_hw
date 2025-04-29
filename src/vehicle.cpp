#include "../include/vehicle.hpp"
#include "../include/database.hpp"
#include "../include/utils.hpp"
#include <sstream>
#include <iostream>

bool VehicleManager::getDuration(const std::string& plate, const std::string& time, std::string& duration, double& fee, std::string& msg) {
    auto& db = Database::getInstance();
    auto vehicles = db.getVehicles();
    auto& v = vehicles[plate];

    std::ifstream config_file("config.json");
    if (!config_file.is_open()) {
        msg = "无法打开配置文件";
        return false;
    }
    json config;
    try {
        config_file >> config;
    } catch (const json::parse_error& e) {
        msg = "配置文件错误";
        return false;
    }
    int freeTime = config["freetime"];
    int stageTime = config["fee_stage_time"];
    double stagePrice = config["fee_stage_price"];
    double dayTop = config["fee_day_top"];

    if (!vehicles.contains(plate) || v["is_inside"] != true) {
        return false;
    }

    double fee;
    std::string effective_entry = v["entry_time"];
    if (v["is_monthly"] == true) {
        std::string monthly_expiry = v["monthly_expiry"];
        if (time <= monthly_expiry) {
            fee = 0.0;
            goto TIMECALC;
        } else {
            // 月卡已过期：重新计算计费起始时间，入场时间和月卡到期时间中取较晚
            effective_entry = (v["entry_time"].get<std::string>() > monthly_expiry) ? v["entry_time"].get<std::string>() : monthly_expiry;
        }
    }
    TIMECALC:
    double totalHours = utils::calculateHours(effective_entry, time);
    int totalSec = static_cast<int>(totalHours * 3600 + 0.5);
    int totalMin = (totalSec + 59) / 60;

    int chargeableMinutes = (totalMin > freeTime) ? (totalMin - freeTime) : 0;
    int days = chargeableMinutes / (24 * 60);
    int remainder = chargeableMinutes % (24 * 60);
    int stages = (remainder + stageTime - 1) / stageTime;
    if (fee != 0.0) {fee = days * dayTop + stages * stagePrice;}

    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << h << ":"
        << std::setw(2) << std::setfill('0') << m << ":"
        << std::setw(2) << std::setfill('0') << s;
    duration = oss.str();
    return true;
}

bool VehicleManager::entry(const std::string& plate, const std::string& time, std::string& msg) {
    auto& db = Database::getInstance();
    auto vehicles = db.getVehicles();

    if (vehicles.contains(plate)) {
        auto& v = vehicles[plate];
        if (v["is_inside"] == true) {
            msg = "车辆已经在场";
            return false;
        }
        if (v["is_blacklisted"] == true) {
            msg = "黑名单车辆";
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
        msg = "入场成功";
        return true;
    }
    msg = "数据库错误";
    return false;
}

bool VehicleManager::exit(const std::string& plate, const std::string& time, double& fee, std::string& duration, std::string& msg) {
    auto& db = Database::getInstance();
    auto vehicles = db.getVehicles();
    auto& v = vehicles[plate];
    
    if (!vehicles.contains(plate) || v["is_inside"] != true) {
        msg = "找不到车辆";
        return false;
    }

    //计算费用
    double fee;
    if (v["is_monthly"] == true) {
        std::string monthly_expiry = v["monthly_expiry"];
        // 如果出场时间小于或等于月卡到期时间，则免费出场
        if (time <= monthly_expiry) {
            fee = 0.0;
            v["is_inside"] = false;
            
            if (db.saveVehicles(vehicles)) {
            msg = "出场成功，月卡免费";
            goto CALCTIME;
            }
            msg = "数据库错误";
            return false;
        } else {
            // 月卡已过期：重新计算计费起始时间为入场时间和月卡到期时间中较晚者
            std::string effective_entry = (v["entry_time"].get<std::string>() > monthly_expiry) ? v["entry_time"].get<std::string>() : monthly_expiry;
            v["entry_time"] = effective_entry;
            v["is_monthly"] = false;
        }
    }

    CALCTIME:
    getDuration(plate, time, duration, fee, msg);

    if (v["is_inside"] == false) {
        v["entry_time"] = "";
        v["history_exits"].push_back(time);
        return true;
    }

    v["is_inside"] = false;
    v["entry_time"] = "";
    v["history_exits"].push_back(time);
    if (db.saveVehicles(vehicles)) {
        msg = "出场成功";
        return true;
    }
    msg = "数据库错误";
    return false;
}

bool VehicleManager::addMonthly(const std::string& plate, int days, std::string& msg) {
    auto& db = Database::getInstance();
    auto vehicles = db.getVehicles();

    if (!vehicles.contains(plate)) {
        vehicles[plate] = {
            {"license_plate", plate},
            {"is_inside", false},
            {"is_monthly", false},
            {"is_blacklisted", false},
            {"entry_time", ""},
            {"history_entries", json::array()},
            {"history_exits", json::array()}
        };
    }
    auto& v = vehicles[plate];

    // 获取当前时间
    std::time_t now = std::time(nullptr);
    std::time_t baseTime = now;

    std::cout << days << std::endl;

    // 如果已有未过期的月卡，则以原到期时间作为基准
    if (v.contains("monthly_expiry") && v["monthly_expiry"].is_string()) {
        std::istringstream iss(v["monthly_expiry"].get<std::string>());
        std::tm tm = {};
        if (iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S")) {
            std::time_t expiryTime = std::mktime(&tm);
            if (expiryTime > now) {
                baseTime = expiryTime;
            }
        }
    }

    // 计算新的到期时间
    std::time_t newExpiry = baseTime + days * 24 * 3600;
    char buffer[20];
    std::tm* newTm = std::localtime(&newExpiry);
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", newTm);

    v["is_monthly"] = true;
    v["monthly_expiry"] = std::string(buffer);

    if (db.saveVehicles(vehicles)) {
        msg = "成功添加月卡天数";
        return true;
    }
    msg = "数据库错误";
    return false;
}

bool VehicleManager::addBlacklist(const std::string& plate, std::string& msg) {
    auto& db = Database::getInstance();
    auto vehicles = db.getVehicles();

    if (!vehicles.contains(plate)) {
        vehicles[plate] = {
            {"license_plate", plate},
            {"is_inside", false},
            {"is_monthly", false},
            {"is_blacklisted", false},
            {"entry_time", ""},
            {"history_entries", json::array()},
            {"history_exits", json::array()}
        };
    }
    auto& v = vehicles[plate];

    if (v["is_blacklisted"] == true) {
        msg = "这辆车已经在黑名单了";
        return false;
    }

    v["is_blacklisted"] = true;

    if (db.saveVehicles(vehicles)) {
        msg = "成功将这辆车添加到黑名单";
        return true;
    }
    msg = "数据库错误";
    return false;
}

bool VehicleManager::removeBlacklist(const std::string& plate, std::string& msg) {
    auto& db = Database::getInstance();
    auto vehicles = db.getVehicles();

    if (!vehicles.contains(plate)) {
        msg = "这辆车不在黑名单中";
        return false;
    }
    auto& v = vehicles[plate];

    if (v["is_blacklisted"] == false) {
        msg = "这辆车不在黑名单中";
        return false;
    }

    v["is_blacklisted"] = false;

    if (db.saveVehicles(vehicles)) {
        msg = "成功将这辆车从黑名单中移除";
        return true;
    }
    msg = "数据库错误";
    return false;
}
