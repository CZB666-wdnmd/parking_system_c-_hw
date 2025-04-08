#include "../include/auth.hpp"
#include "../include/database.hpp"
#include "../include/utils.hpp"
#include <random>
#include <sstream>
#include <iomanip>

// 该函数使用单例模式确保全局只有一个 Auth 实例
Auth& Auth::getInstance() {
    static Auth instance;
    return instance;
}

// 生成随机的临时认证令牌
std::string Auth::generateToken() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        int random = dis(gen);
        ss << std::hex << std::setw(2) << std::setfill('0') << random;
    }
    return ss.str();
}

// 验证用户登录信息
bool Auth::loginUser(const std::string& username, const std::string& password, std::string& token, std::string& role) {
    auto& db = Database::getInstance();
    auto users = db.getUsers();

    // 查找用户
    for (const auto& user : users) {
        if (user["username"] == username) {
            std::string storedAuth = user["auth"];
            std::string userRole = user["role"];

            // bot用户直接比较密钥
            if (userRole == "bot") {
                if (password == storedAuth) {
                    token = generateToken();
                    {
                        std::lock_guard<std::mutex> lock(tokensMutex);
                        tokens[token] = {userRole, username};
                    }
                    role = userRole;
                    return true;
                }
            } else {
                // 对用户密码进行 SHA256 哈希后再与存储的认证码比较
                std::string inputHash = utils::sha256(password);
                if (inputHash == storedAuth) {
                    // 验证成功, 生成新的令牌, 并输出令牌及用户角色
                    token = generateToken();
                    {
                        std::lock_guard<std::mutex> lock(tokensMutex);
                        tokens[token] = {userRole, username};
                    }
                    role = userRole;
                    return true;
                }
            }
            break;
        }
    }
    return false;
}

// 验证令牌的有效性
bool Auth::validateToken(const std::string& token, std::string& role, std::string& username) {
    std::lock_guard<std::mutex> lock(tokensMutex);
    auto it = tokens.find(token);
    if (it != tokens.end()) {
        role = it->second.first;
        username = it->second.second;
        return true;
    }
    return false;
}

// 用户登出，移除指定的令牌
void Auth::removeToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(tokensMutex);
    tokens.erase(token);
}