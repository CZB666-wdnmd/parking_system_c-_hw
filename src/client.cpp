#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <utility>
#include <thread>

#include <termios.h>
#include <unistd.h>

using json = nlohmann::json;
using namespace std;

// 全局变量存储 token 与用户角色
std::string g_token;
std::string g_role;

// 清屏函数
void clearScreen() {
    system("clear");
}

// 加载配置文件
std::pair<std::string, int> loadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "无法打开配置文件: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }
    json j;
    file >> j;
    std::string ip = j.at("ip");
    int port = j.at("port");
    return {ip, port};
}

// cURL 回调函数
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// HTTP 请求封装函数
json http_request(const std::pair<std::string, int>& cfg,
                  const std::string& url,
                  const std::string& method,
                  const std::string& auth,
                  const json& payload) {
    std::string fullUrl = "http://" + cfg.first + ":" + std::to_string(cfg.second) + url;
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "CURL 初始化失败" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::string response_data;
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10秒超时

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: " + auth).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::string payloadStr;
    if (method == "POST" || method == "post") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        payloadStr = payload.dump();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payloadStr.size());
    } else if (method == "GET" || method == "get") {
        if (!payload.empty()) {
            std::string query = "?";
            bool first = true;
            for (auto& el : payload.items()) {
                if (!first)
                    query += "&";
                std::string value_str = el.value().is_string() ?
                                        el.value().get<std::string>() : el.value().dump();
                char* encoded = curl_easy_escape(curl, value_str.c_str(), value_str.length());
                query += el.key() + "=" + std::string(encoded);
                curl_free(encoded);
                first = false;
            }
            fullUrl += query;
            curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
        }
    }

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "请求失败: " << curl_easy_strerror(res) << std::endl;
        exit(EXIT_FAILURE);
    }
    try {
        return json::parse(response_data);
    } catch (const std::exception& e) {
        std::cerr << "JSON 解析错误: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
}

// 隐藏密码输入
std::string getHiddenInput(const std::string& prompt) {
    std::string password;
    cout << prompt;
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    getline(cin, password);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    cout << endl;
    return password;
}

// 检测服务器是否正常
bool isServerAlive(const std::pair<std::string, int>& cfg) {
    try {
        auto response = http_request(cfg, "/api/alive", "GET", "", {});
        return response.contains("status") && response["status"] == "ok";
    } catch (const std::exception& e) {
        std::cerr << "服务器检测失败: " << e.what() << std::endl;
        return false;
    }
}

// 登录界面
bool loginUI(const std::pair<std::string, int>& cfg) {
    clearScreen();
    std::string username, password;
    cout << "==== 停车场管理系统 ====" << endl;
    cout << "请输入用户名: ";
    getline(cin, username);
    password = getHiddenInput("请输入密码（已隐藏输入）: ");
    json payload = {{"username", username}, {"password", password}};
    auto r = http_request(cfg, "/api/auth/login", "POST", "", payload);
    if (r.contains("token")) {
        g_token = r["token"];
        g_role = r["role"];
        cout << "\n登录成功！" << endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return true;
    } else {
        cout << "\n登录失败: " << r.dump() << endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return false;
    }
}

// 车辆操作界面
void vehicleOperation(const std::pair<std::string, int>& cfg) {
    clearScreen();
    cout << "==== 车辆操作 ====" << endl;
    cout << "请输入车牌号: ";
    std::string plate;
    getline(cin, plate);
    cout << "请选择操作 (1. 进场  2. 出场  3. 添加月卡  4. 加入黑名单  5. 移除黑名单): ";
    std::string op;
    getline(cin, op);
    std::string action;
    if (op == "1") {
        action = "entry";
    } else if (op == "2") {
        action = "exit";
    } else if (op == "3") {
        action = "addMonthly";
    } else if (op == "4") {
        action = "addBlacklist";
    } else if (op == "5") {
        action = "removeBlacklist";
    } else {
        cout << "\n无效的操作选择。" << endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return;
    }
    
    if (action == "addMonthly") {
        cout << "请输入月卡天数: ";
        int days;
        std::string daysInput;
        getline(cin, daysInput);
        try {
            days = std::stoi(daysInput);
            if (days <= 0) {
            cout << "\n无效的天数，请输入一个正整数。" << endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return;
            }
        } catch (const std::exception&) {
            cout << "\n输入错误，请输入一个有效的整数。" << endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return;
        }
        cout << "\n确认操作: " << plate << " 添加月卡 " << days << "天? (y/n): ";
        std::string confirm;
        getline(cin, confirm);
        if (confirm != "y" && confirm != "Y") {
            cout << "\n已取消操作。" << endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return;
        }
        json payload = {{"license_plate", plate}, {"action", action}, {"days", days}};
        auto r = http_request(cfg, "/api/admin/vehicle", "POST", g_token, payload);
        if (r.contains("result") && r["result"] == "success") {
            cout << "\n操作成功: " << r["message"] << endl;
        } else {
            cout << "\n操作失败: " << r["message"] << endl;
        }
    } else if (action == "addBlacklist") {
        json payload = {{"license_plate", plate}, {"action", action}};
        auto r = http_request(cfg, "/api/admin/vehicle", "POST", g_token, payload);
        if (r.contains("result") && r["result"] == "success") {
            cout << "\n操作成功: " << r["message"] << endl;
        } else {
            cout << "\n操作失败: " << r["message"] << endl; 
        }
    } else if (action == "removeBlacklist") {
        json payload = {{"license_plate", plate}, {"action", action}};
        auto r = http_request(cfg, "/api/admin/vehicle", "POST", g_token, payload);
        if (r.contains("result") && r["result"] == "success") {
            cout << "\n操作成功: " << r["message"] << endl;
        } else {
            cout << "\n操作失败: " << r["message"] << endl;
        }
    } else {
        cout << "\n确认操作: " << plate << " " << (action == "entry" ? "进场" : "出场") << "? (y/n): ";
        std::string confirm;
        getline(cin, confirm);
        if (confirm != "y" && confirm != "Y") {
            cout << "\n已取消操作。" << endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return;
        }
        json payload = {{"license_plate", plate}, {"action", action}};
        auto r = http_request(cfg, "/api/admin/vehicle", "POST", g_token, payload);
        if (r.contains("result") && r["result"] == "success" && action == "entry") {
            cout << "\n操作成功: " << r["message"] << endl;
        } else if (r.contains("result") && r["result"] == "success" && action == "exit") {
            cout << "\n操作成功: " << r["message"] << "停车时长：" << r["parking_duration"] << "计费：" << r["fee"] << endl;
        } else {
            cout << "\n操作失败: " << r["message"] << endl;
        }
    }
    
    cout << "\n按回车键返回首页...";
    // 等待用户按下回车键
    cin.get();
}

// 车辆信息查询界面
void vehicleInfo(const std::pair<std::string, int>& cfg) {
    clearScreen();
    cout << "==== 车辆信息查询 ====" << endl;
    cout << "请输入车牌号: ";
    std::string plate;
    getline(cin, plate);
    auto r = http_request(cfg, "/api/vehicles/" + plate, "GET", g_token, {});
    if (r.contains("license_plate")) {
        cout << "\n车牌号: " << r["license_plate"] << endl;
        cout << "目前停车时长" << r["duration"] << endl;
        cout << "目前停车费用: " << r["fee"] << endl;
        cout << "是否在场: " << (r["is_inside"] ? "是" : "否") << endl;
        cout << "是否月卡用户: " << (r["is_monthly"] ? "是" : "否") << endl;
        if (r["is_monthly"]) {
            cout << "月卡到期时间: " << r["monthly_expiry"] << endl;
        }
        cout << "是否黑名单: " << (r["is_blacklisted"] ? "是" : "否") << endl;
        if (r.contains("entry_time") && !r["entry_time"].is_null()) {
            cout << "入场时间: " << r["entry_time"] << endl;
        }
        if (r.contains("history_entries") && r["history_entries"].is_array()) {
            cout << "历史入场记录: ";
            for (const auto& entry : r["history_entries"]) {
                cout << entry << " ";
            }
            cout << endl;
        }
        if (r.contains("history_exits") && r["history_exits"].is_array()) {
            cout << "历史出场记录: ";
            for (const auto& exit : r["history_exits"]) {
                cout << exit << " ";
            }
            cout << endl;
        }
    } else {
        cout << "\n查询失败: " << r.dump() << endl;
    }
    cout << "\n按回车键退出...";
    cin.get();
}

// 显示所有车牌
void listAllVehicles(const std::pair<std::string, int>& cfg) {
    clearScreen();
    cout << "==== 所有车牌 ====" << endl;
    auto r = http_request(cfg, "/api/vehicles", "GET", g_token, {});
    if (r.contains("plates") && r["plates"].is_array()) {
        for (const auto& plate : r["plates"]) {
            cout << plate << endl;
        }
    } else {
        cout << "\n查询失败: " << r.dump() << endl;
    }
    cout << "\n按回车键退出...";
    cin.get();
}

// 显示当前在场车辆
void listInsideVehicles(const std::pair<std::string, int>& cfg) {
    clearScreen();
    cout << "==== 当前在场车辆 ====" << endl;
    auto r = http_request(cfg, "/api/vehicles_inside", "GET", g_token, {});
    if (r.contains("plates") && r["plates"].is_array()) {
        for (const auto& plate : r["plates"]) {
            cout << plate << endl;
        }
    } else {
        cout << "\n查询失败: " << r.dump() << endl;
    }
    cout << "\n按回车键退出...";
    cin.get();
}

// 退出登录
void logout(const std::pair<std::string, int>& cfg) {
    clearScreen();
    auto r = http_request(cfg, "/api/auth/logout", "POST", g_token, {{"token", g_token}});
    cout << "退出登录: " << r.dump() << endl;
    g_token.clear();
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
  
// 管理员界面
void adminMenu(const std::pair<std::string, int>& cfg) {
    while (true) {
        clearScreen();
        cout << "==== 管理界面 ====" << endl;
        cout << "1. 车辆操作" << endl;
        cout << "2. 查看当前在场车辆" << endl;
        cout << "3. 车辆信息查询" << endl;
        cout << "4. 显示所有车牌" << endl;
        cout << "5. 退出登录" << endl;
        cout << "请选择操作 (1-5): ";
        
        std::string choice;
        getline(cin, choice);
        
        if (choice == "1")
            vehicleOperation(cfg);
        else if (choice == "2")
            listInsideVehicles(cfg);
        else if (choice == "3")
            vehicleInfo(cfg);
        else if (choice == "4")
            listAllVehicles(cfg);
        else if (choice == "5") {
            logout(cfg);
            break;
        } else {
            cout << "\n无效的选择，请重试。" << endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

// 用户界面
void userMenu(const std::pair<std::string, int>& cfg) {
    while (true) {
        clearScreen();
        cout << "==== 用户界面 ====" << endl;
        cout << "1. 车辆信息查询" << endl;
        cout << "2. 查看当前在场车辆" << endl;
        cout << "3. 退出登录" << endl;
        cout << "请选择操作 (1-3): ";
        std::string choice;
        getline(cin, choice);
        if (choice == "1")
            vehicleInfo(cfg);
        else if (choice == "2")
            listInsideVehicles(cfg);
        else if (choice == "3") {
            logout(cfg);
            break;
        } else {
            cout << "\n无效的选择，请重试。" << endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

int main() {
    auto cfg = loadConfig("config_client.json");
    cout << "正在检测服务器状态..." << endl;
    if (!isServerAlive(cfg)) {
        cout << "服务器不可用，请检查配置或服务器状态。" << endl;
        return 1;
    }
    cout << "服务器正常，继续登录。" << endl;
    while (true) {
        if (!loginUI(cfg)) {
            cout << "\n登录失败。是否重试？(y/n): ";
            std::string ans;
            getline(cin, ans);
            if (ans != "y" && ans != "Y")
                break;
            else
                continue;
        }
        if (g_role == "admin")
            adminMenu(cfg);
        else if (g_role == "user")
            userMenu(cfg);
        else
            cout << "未知的用户角色：" << g_role << endl;
        cout << "\n是否重新登录？(y/n): ";
        std::string ans;
        getline(cin, ans);
        if (ans != "y" && ans != "Y")
            break;
    }
    cout << "程序退出。" << endl;
    return 0;
}
