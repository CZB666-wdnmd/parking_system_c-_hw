#pragma once
#include <string>
#include <map>
#include <mutex>

class Auth {
public:
    static Auth& getInstance();
    std::string generateToken();
    bool validateToken(const std::string& token, std::string& role, std::string& username);
    bool loginUser(const std::string& username, const std::string& password, std::string& token, std::string& role);
    void removeToken(const std::string& token);

private:
    Auth() = default;
    std::mutex tokensMutex;
    std::map<std::string, std::pair<std::string, std::string>> tokens;
};