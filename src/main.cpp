#include "../include/auth.hpp"
#include "../include/database.hpp"
#include "../include/logger.hpp"
#include "../include/vehicle.hpp"
#include "httplib.h"
#include <nlohmann/json.hpp>
#include "../include/utils.hpp"

#include <iostream>
using json = nlohmann::json;

void setupRoutes(httplib::Server& svr) {
    // 状态检测
    svr.Get("/api/alive", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status": "ok"})", "application/json");
    });
    // 用户登录
    svr.Post("/api/auth/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string user = body["username"];
            std::string pass = body["password"];

            std::string token, role;
            if (Auth::getInstance().loginUser(user, pass, token, role)) {
                json response = {{"token", token}, {"role", role}};
                res.set_content(response.dump(), "application/json");
                Logger::logUser(user, "login", "Login success");
            } else {
                res.status = 401;
                res.set_content(json{{"error", "Invalid credentials"}}.dump(), "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content(json{{"error", "Bad request"}}.dump(), "application/json");
        }
    });

    // 退出登入
    svr.Post("/api/auth/logout", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string token = body["token"];

            Auth::getInstance().removeToken(token);
            res.set_content(json{{"result", "Logged out successfully"}}.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(json{{"error", "Bad request"}}.dump(), "application/json");
        }
    });

    // OpenCV 接口
    svr.Post("/api/opencv/process", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string token = body["token"];
            std::string plate = body["license_plate"];
            std::string action = body["action"];
            std::string time = body.value("timestamp", utils::getCurrentTimeISO());

            // 从数据库验证bot token
            auto users = Database::getInstance().getUsers();
            bool validBot = false;
            std::string botUsername;

            for (const auto& user : users) {
                if (user["role"] == "bot" && user["auth"] == token) {
                    validBot = true;
                    botUsername = user["username"];
                    break;
                }
            }

            if (!validBot) {
                res.status = 401;
                res.set_content(json{{"error", "Invalid bot token"}}.dump(), "application/json");
                return;
            }

            // 处理车辆进出
            if (action == "entry") {
                std::string msg;
                if (VehicleManager::entry(plate, time, msg)) {
                    Logger::logVehicle(plate, "entry", "[Bot:" + botUsername + "] " + msg);
                    res.set_content(json{{"result", "success"}, {"message", msg}}.dump(), "application/json");
                } else {
                    Logger::logVehicle(plate, "entry", "[Bot:" + botUsername + "] Failed: " + msg);
                    res.set_content(json{{"result", "fail"}, {"message", msg}}.dump(), "application/json");
                }
            } else if (action == "exit") {
                double fee;
                std::string duration, msg;
                if (VehicleManager::exit(plate, time, fee, duration, msg)) {
                    json response = {
                        {"result", "success"},
                        {"parking_duration", duration},
                        {"fee", fee},
                        {"message", msg}
                    };
                    Logger::logVehicle(plate, "exit", "[Bot:" + botUsername + "] " + msg);
                    res.set_content(response.dump(), "application/json");
                } else {
                    Logger::logVehicle(plate, "exit", "[Bot:" + botUsername + "] Failed: " + msg);
                    res.set_content(json{{"result", "fail"}, {"message", msg}}.dump(), "application/json");
                }
            } else {
                res.status = 400;
                res.set_content(json{{"error", "Invalid action"}}.dump(), "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content(json{{"error", "Bad request"}}.dump(), "application/json");
        }
    });

    // 获取单车辆信息
    svr.Get("/api/vehicles/(.*)", [](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.get_header_value("Authorization");
        std::string role, user;
        if (!Auth::getInstance().validateToken(token, role, user)) {
            res.status = 401;
            res.set_content(json{{"error", "Unauthorized"}}.dump(), "application/json");
            return;
        }

        std::string plate = req.matches[1];
        auto vehicles = Database::getInstance().getVehicles();
        double fee;
        std::string duration, msg;
        std::string time = utils::getCurrentTimeISO();
        json vehicle_info;
        if (vehicles.contains(plate)) {
            vehicle_info = vehicles[plate];
            if (vehicle_info["is_inside"] == true) {
                VehicleManager::getDuration(plate, time, duration, fee, msg);
                vehicle_info["duration"] = duration;
                vehicle_info["fee"] = fee;
            } else {
                res.status = 500;
                res.set_content(json{{"error", "Internal Server Error"}}.dump(), "application/json");
            }
            res.set_content(vehicle_info.dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content(json{{"error", "Not found"}}.dump(), "application/json");
        }
    });

    // 获取所有车牌 (非bot用户可访问)
    svr.Get("/api/vehicles", [](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.get_header_value("Authorization");
        std::string role, username;
        if (!Auth::getInstance().validateToken(token, role, username)) {
            res.status = 401;
            res.set_content(json{{"error", "Unauthorized"}}.dump(), "application/json");
            return;
        }

        if (role == "bot") {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden for bot users"}}.dump(), "application/json");
            return;
        }

        auto vehicles = Database::getInstance().getVehicles();
        json plates = json::array();
        for (auto& [plate, _] : vehicles.items()) {
            plates.push_back(plate);
        }

        res.set_content(json{{"plates", plates}}.dump(), "application/json");
    });

    // 获取已入场车牌 (非bot用户可访问)
    svr.Get("/api/vehicles_inside", [](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.get_header_value("Authorization");
        std::string role, username;
        if (!Auth::getInstance().validateToken(token, role, username)) {
            res.status = 401;
            res.set_content(json{{"error", "Unauthorized"}}.dump(), "application/json");
            return;
        }

        if (role == "bot") {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden for bot users"}}.dump(), "application/json");
            return;
        }

        auto vehicles = Database::getInstance().getVehicles();
        json inside_plates = json::array();
        for (auto& [plate, data] : vehicles.items()) {
            if (data["is_inside"] == true) {
                inside_plates.push_back(plate);
            }
        }

        res.set_content(json{{"plates", inside_plates}}.dump(), "application/json");
    });

    // 管理车辆
    svr.Post("/api/admin/vehicle", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string token = req.get_header_value("Authorization");
            std::string role, username;
            if (!Auth::getInstance().validateToken(token, role, username) || role != "admin") {
                res.status = 403;
                res.set_content(json{{"error", "Forbidden: Admin access required"}}.dump(), "application/json");
                return;
            }

            auto body = json::parse(req.body);
            std::string plate = body["license_plate"];
            std::string action = body["action"];
            std::string time = body.value("timestamp", utils::getCurrentTimeISO());

            if (action == "entry") {
                std::string msg;
                if (VehicleManager::entry(plate, time, msg)) {
                    Logger::logVehicle(plate, "admin_entry", "[Admin:" + username + "] " + msg);
                    res.set_content(json{{"result", "success"}, {"message", msg}}.dump(), "application/json");
                } else {
                    Logger::logVehicle(plate, "admin_entry", "[Admin:" + username + "] Failed: " + msg);
                    res.set_content(json{{"result", "fail"}, {"message", msg}}.dump(), "application/json");
                }
            } else if (action == "exit") {
                double fee;
                std::string duration, msg;
                if (VehicleManager::exit(plate, time, fee, duration, msg)) {
                    json response = {
                        {"result", "success"},
                        {"parking_duration", duration},
                        {"fee", fee},
                        {"message", msg}
                    };
                    Logger::logVehicle(plate, "admin_exit", "[Admin:" + username + "] " + msg);
                    res.set_content(response.dump(), "application/json");
                } else {
                    Logger::logVehicle(plate, "admin_exit", "[Admin:" + username + "] Failed: " + msg);
                    res.set_content(json{{"result", "fail"}, {"message", msg}}.dump(), "application/json");
                }
            } else if (action == "addMonthly") {
                int days = body["days"];
                std::cout << days << std::endl;
                std::string msg;
                if (VehicleManager::addMonthly(plate, days, msg)) {
                    Logger::logVehicle(plate, "add_monthly", "[Admin:" + username + "] " + msg);
                    res.set_content(json{{"result", "success"}, {"message", msg}}.dump(), "application/json");
                } else {
                    Logger::logVehicle(plate, "add_monthly", "[Admin:" + username + "] Failed: " + msg);
                    res.set_content(json{{"result", "fail"}, {"message", msg}}.dump(), "application/json");
                }
            } else if (action == "addBlacklist") {
                std::string msg;
                if (VehicleManager::addBlacklist(plate, msg)) {
                    Logger::logVehicle(plate, "add_blacklist", "[Admin:" + username + "] " + msg);
                    res.set_content(json{{"result", "success"}, {"message", msg}}.dump(), "application/json");
                } else {
                    Logger::logVehicle(plate, "add_blacklist", "[Admin:" + username + "] Failed: " + msg);
                    res.set_content(json{{"result", "fail"}, {"message", msg}}.dump(), "application/json");
                }
            } else if (action == "removeBlacklist") {
                std::string msg;
                if (VehicleManager::removeBlacklist(plate, msg)) {
                    Logger::logVehicle(plate, "remove_blacklist", "[Admin:" + username + "] " + msg);
                    res.set_content(json{{"result", "success"}, {"message", msg}}.dump(), "application/json");
                } else {
                    Logger::logVehicle(plate, "remove_blacklist", "[Admin:" + username + "] Failed: " + msg);
                    res.set_content(json{{"result", "fail"}, {"message", msg}}.dump(), "application/json");
                }
            } else {
                res.status = 400;
                res.set_content(json{{"error", "Invalid action"}}.dump(), "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content(json{{"error", "Bad request"}}.dump(), "application/json");
        }
    });
}

bool checkVehiclesJSON() {
    std::ifstream vehicles_file("vehicles.json");
    if (!vehicles_file.is_open()) {
        std::cout << "vehicles.json not found, creating a new one." << std::endl;
        std::ofstream emptyFile("vehicles.json");
        if (emptyFile.is_open()) {
            emptyFile << "{}";
            emptyFile.close();
        } else {
            std::cerr << "Error: Could not create vehicles.json." << std::endl;
        }
    }
    return true;
}

bool checkUsersJSON() {
    std::ifstream users_file("users.json");
    if (!users_file.is_open()) {
        std::cerr << "users.json not found, please create it yourself." << std::endl;
        return false;
    }
    return true;
}

bool checkConfigJSON() {
    std::ifstream config_file("config.json");
    if (!config_file.is_open()) {
        std::cerr << "config.json not found, please create it yourself." << std::endl;
        return false;
    }
    return true;
}

int main() {
    // 检查配置文件
    if (!checkVehiclesJSON() || !checkUsersJSON() || !checkConfigJSON()) {
        return 1;
    }
    httplib::Server svr;
    setupRoutes(svr);
    std::ifstream config_file("config.json");
    json config;
    if (config_file.is_open()) {
        config_file >> config;
    } else {
        std::cerr << "Error: Could not open config.json. Exiting.\n";
        exit(EXIT_FAILURE);
    }
    if (!config.contains("ip") || !config.contains("port")) {
        std::cerr << "Error: config.json must include ip and port. Exiting.\n";
        exit(EXIT_FAILURE);
    }
    std::string ip = config["ip"];
    int port = config["port"];

    svr.listen(ip.c_str(), port);
    return 0;
}