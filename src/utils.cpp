#include "../include/utils.hpp"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <ctime>

// 计算SHA256哈希值
std::string utils::sha256(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, input.c_str(), input.size());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

// 获取当前时间的ISO格式字符串
std::string utils::getCurrentTimeISO() {
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm);
    return std::string(buf);
}

// 将ISO格式字符串转换为时间戳
time_t utils::isoStringToTime(const std::string& iso) {
    std::tm tm = {};
    std::istringstream ss(iso);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return mktime(&tm);
}

// 计算两个ISO格式时间字符串之间的小时差
double utils::calculateHours(const std::string& start, const std::string& end) {
    time_t t1 = isoStringToTime(start);
    time_t t2 = isoStringToTime(end);
    return difftime(t2, t1) / 3600.0;
}