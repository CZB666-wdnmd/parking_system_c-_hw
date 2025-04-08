#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <utility>
#include <iostream>

using namespace ftxui;
using json = nlohmann::json;

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

#include <curl/curl.h>

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

json http_request(const std::pair<std::string, int>& cfg,
                  const std::string& url,
                  const std::string& method,
                  const std::string& auth,
                  const json& payload) {
    std::string fullUrl = "http://" + cfg.first + ":" + std::to_string(cfg.second) + url;
    CURL* curl = curl_easy_init();
    if (!curl) {
            std::cerr << "CURL init failed" << std::endl;
            exit(EXIT_FAILURE);
    }
    std::string response_data;
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    // Hardcoded HTTP proxy configuration.
    curl_easy_setopt(curl, CURLOPT_PROXY, "http://127.0.0.1:9001");

    // Set headers including Authorization and JSON content type.
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: " + auth).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if (method == "POST" || method == "post") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            std::string payloadStr = payload.dump();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
    } else if (method == "GET" || method == "get") {
            // If payload is provided for a GET, append as query parameters.
            if (!payload.empty()) {
                    std::string query = "?";
                    bool first = true;
                    for (auto& el : payload.items()) {
                            if (!first) {
                                    query += "&";
                            }
                            query += el.key() + "=" + el.value().dump();
                            first = false;
                    }
                    fullUrl += query;
                    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
            }
    } else {
            std::cerr << "Unsupported method: " << method << std::endl;
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            exit(EXIT_FAILURE);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
            std::cerr << "Request failed: " << curl_easy_strerror(res) << std::endl;
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            exit(EXIT_FAILURE);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    json response_json;
    try {
            response_json = json::parse(response_data);
    } catch (const std::exception& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
            exit(EXIT_FAILURE);
    }
    return response_json;
}

static std::string g_token;
static std::string g_role;

Component MakeLoginUI(const std::pair<std::string, int>& cfg, ScreenInteractive &screen) {
    auto username = std::make_shared<std::string>();
    auto password = std::make_shared<std::string>();
    auto output   = std::make_shared<std::string>();

    auto loginBtn = Button("Login", [=, &screen] {
        json payload = {{"username", *username}, {"password", *password}};
        auto r = http_request(cfg, "/api/auth/login", "POST", "", payload);
        std::cout << payload.dump() << std::endl;
        if(r.contains("token")) {
            g_token = r["token"].get<std::string>();
            g_role  = r["role"].get<std::string>();
            *output = "Login OK. Token=" + g_token;
            screen.Exit(); // Exit login loop to switch UI
        } else {
            *output = r.dump();
        }
    });

    auto quitBtn = Button("Quit", [] {
        std::exit(EXIT_SUCCESS);
    });

    auto container = Container::Vertical({
        Input(username.get(), "Username"),
        Input(password.get(), "Password"),
        loginBtn,
        quitBtn
    });

    auto renderer = Renderer(container, [=] {
        return vbox({
            text("Login Interface") | bold,
            separator(),
            text("Username:"),
            text(*username),
            text("Password:"),
            text(*password),
            separator(),
            hbox({
                loginBtn->Render(),
                text(" "),
                quitBtn->Render()
            }),
            separator(),
            text("Output:"),
            text(*output) | size(HEIGHT, GREATER_THAN, 3) | size(WIDTH, EQUAL, 60),
        }) | border;
    });

    return renderer;
}

Component MakeOperationUI(const std::pair<std::string, int>& cfg) {
    auto plate    = std::make_shared<std::string>();
    auto action   = std::make_shared<std::string>("entry");
    auto output   = std::make_shared<std::string>();

    auto logoutBtn = Button("Logout", [=] {
        json payload = {{"token", g_token}};
        auto r = http_request(cfg, "/api/auth/logout", "POST", g_token, payload);
        g_token.clear();
        g_role.clear();
    });

    auto getOneBtn = Button("Get Vehicle", [=] {
        std::string url = "/api/vehicles/" + *plate;
        auto r = http_request(cfg, url, "GET", g_token, {});
        *output = r.dump();
    });

    auto getAllBtn = Button("Get Plates", [=] {
        auto r = http_request(cfg, "/api/vehicles", "GET", g_token, {});
        *output = r.dump();
    });

    auto getInsideBtn = Button("Get Inside", [=] {
        auto r = http_request(cfg, "/api/vehicles/inside", "GET", g_token, {});
        *output = r.dump();
    });

    auto adminBtn = Button("Admin Vehicle", [=] {
        json payload = {
            {"license_plate", *plate},
            {"action", *action}
        };
        auto r = http_request(cfg, "/api/admin/vehicle", "POST", g_token, payload);
        *output = r.dump();
    });

    auto container = Container::Vertical({
        Input(plate.get(), "License Plate"),
        Input(action.get(), "entry or exit"),
        logoutBtn,
        adminBtn,
        getOneBtn,
        getAllBtn,
        getInsideBtn,
    });

    auto renderer = Renderer(container, [=] {
        return vbox({
            text("Operation Interface") | bold,
            separator(),
            text("Plate:"),
            text(*plate),
            text("Action (entry/exit):"),
            text(*action),
            separator(),
            hbox({adminBtn->Render(), text(" "), getOneBtn->Render()}),
            hbox({getAllBtn->Render(), text(" "), getInsideBtn->Render()}),
            separator(),
            hbox({logoutBtn->Render()}),
            separator(),
            text("Output:"),
            text(*output) | size(HEIGHT, GREATER_THAN, 3) | size(WIDTH, EQUAL, 60),
        }) | border;
    });

    return renderer;
}

int main() {
    auto cfg = loadConfig("config_client.json");

    ScreenInteractive screen = ScreenInteractive::TerminalOutput();
    auto loginUI = MakeLoginUI(cfg, screen);
    screen.Loop(loginUI);

    if (!g_token.empty()) {
        auto operationUI = MakeOperationUI(cfg);
        screen.Loop(operationUI);
    }

    return 0;
}
