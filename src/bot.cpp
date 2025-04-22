#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <thread>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

// OpenCV
#include <opencv2/opencv.hpp>

// Tesseract OCR
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

using json = nlohmann::json;

// 定义帧宽高
#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480

// cURL 回调函数
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    std::string* str = (std::string*)userp;
    size_t totalSize = size * nmemb;
    str->append((char*)contents, totalSize);
    return totalSize;
}

// HTTP 请求封装函数
bool sendHttpPost(const std::string &ip, const int &port, const std::string &token, 
                  const std::string &license_plate, const std::string &action)
{
    std::string url = "http://" + ip + ":" + std::to_string(port) + "/api/opencv/process";

    // 构建JSON payload
    json payload;
    payload["token"] = token;
    payload["license_plate"] = license_plate;
    payload["action"] = action;

    std::string payloadStr = payload.dump();

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "无法初始化curl" << std::endl;
        return false;
    }

    std::string response_string;
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payloadStr.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "请求失败: " << curl_easy_strerror(res) << std::endl;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

// 处理车牌图像
bool processPlatesImages(const cv::Mat& frame, cv::CascadeClassifier& plateCascade, 
                        std::vector<cv::Rect>& plates, cv::Mat& gray)
{
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);
    plateCascade.detectMultiScale(gray, plates, 1.1, 10, 0, cv::Size(30, 30));
    return true;
}

// 获取车牌字符串
bool getPlate(const std::vector<cv::Rect>& plates, 
            std::vector<std::string>& plateStrings,
            tesseract::TessBaseAPI& ocr,
            cv::Mat& frame,
            const cv::Mat& gray)
{
    for (size_t i = 0; i < plates.size(); i++)
    {
        cv::rectangle(frame, plates[i], cv::Scalar(0, 255, 0), 2);
        cv::Mat plateROI = gray(plates[i]);

        cv::Mat thresh;
        cv::threshold(plateROI, thresh, 0, 255, cv::THRESH_BINARY + cv::THRESH_OTSU);

        ocr.SetImage(thresh.data, thresh.cols, thresh.rows, 1, thresh.step);
        std::string plateText = std::string(ocr.GetUTF8Text());

        std::istringstream iss(plateText);
        std::string word, result;
        while(iss >> word) {
            result += word;
        }
        if (!result.empty()) {
            plateStrings.push_back(result);
        }
    }
    return true;
}

// 初始化OpenCV和Tesseract
bool init(cv::CascadeClassifier &plateCascade, tesseract::TessBaseAPI &ocr, 
         std::vector<unsigned char> &buffer)
{
    const int FRAME_SIZE = FRAME_WIDTH * FRAME_HEIGHT * 3;
    buffer.resize(FRAME_SIZE);
    
    if (!plateCascade.load("haarcascade_russian_plate_number.xml")) {
        std::cerr << "加载车牌级联模型失败" << std::endl;
        return false;
    }

    if (ocr.Init(nullptr, "eng", tesseract::OEM_LSTM_ONLY)) {
        std::cerr << "无法初始化tesseract OCR" << std::endl;
        return false;
    }
    ocr.SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);
    return true;
}

int main(int argc, char** argv)
{
    std::ifstream config_file("config_bot.json");
    if (!config_file.is_open()) {
        std::cerr << "无法打开config_bot.json文件" << std::endl;
        return -1;
    }

    json config;
    try {
        config_file >> config;
    } catch(const json::parse_error &e) {
        std::cerr << "解析config_bot.json出错：" << e.what() << std::endl;
        return -1;
    }

    std::string ip = config["ip"];
    int port = config["port"];
    std::string token = config["token"];
    std::string action = config["role"];

    cv::CascadeClassifier plateCascade;
    tesseract::TessBaseAPI ocr;
    std::vector<unsigned char> buffer;
    if (!init(plateCascade, ocr, buffer)) {
        return -1;
    }

    std::cout << "程序启动，按q退出" << std::endl;
    const int FRAME_SIZE = FRAME_WIDTH * FRAME_HEIGHT * 3;

    bool skipDecte;

    while (true)
    {
        if (!std::cin.read(reinterpret_cast<char*>(buffer.data()), FRAME_SIZE)) {
            std::cerr << "读取帧失败或数据结束" << std::endl;
            break;
        }

        cv::Mat frame(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC3, buffer.data());
        std::vector<cv::Rect> plates;
        cv::Mat gray;

        if (!processPlatesImages(frame, plateCascade, plates, gray)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (plates.empty()) {
            cv::imshow("Video", frame);
            skipDecte = false;
            if (cv::waitKey(30) == 'q') break;
            continue;
        }

        if (skipDecte){continue;}

        std::vector<std::string> plateStrings;
        if (!getPlate(plates, plateStrings, ocr, frame, gray)) {
            continue;
        }

        if (!plateStrings.empty()) {
            std::cout << "检测到车牌: " << plateStrings[0] << std::endl;
            sendHttpPost(ip, port, token, plateStrings[0], action);
            skipDecte = true;
        }

        cv::imshow("Video", frame);
        if (cv::waitKey(30) == 'q') break;
    }

    cv::destroyAllWindows();
    ocr.End();
    return 0;
}