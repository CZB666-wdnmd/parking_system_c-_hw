# 停车场管理系统 (Parking System)

这是一个使用 C++ 开发的停车场管理系统。它包含一个后端服务器、一个命令行客户端以及一个基于 OpenCV 的车牌识别机器人程序。

## 项目组件

本项目包含三个主要的可执行程序：

1.  **`parking_system_server`**:
    * 基于 `httplib` 的 C++ HTTP 服务器。
    * 提供 RESTful API 用于用户认证、车辆进出管理、费用计算、月卡管理、黑名单管理等操作。
    * 使用 JSON 文件 (`users.json`, `vehicles.json`) 存储用户和车辆数据。
    * 通过 `config.json` 进行配置 (如监听地址、端口、计费规则等)。
    * 记录操作日志到控制台和 `system.log` 文件。

2.  **`parking_system_client`**:
    * 一个命令行界面 (CLI) 客户端，用于与服务器 API 交互。
    * 支持用户登录 (区分管理员和普通用户)。
    * 允许执行车辆进/出场、查询车辆信息、查看在场车辆、添加/移除黑名单、添加月卡等操作 (部分操作需要管理员权限)。
    * 使用 `libcurl` 发送 HTTP 请求。
    * 通过 `config_client.json` 配置服务器连接信息。

3.  **`parking_system_bot`**:
    * 一个机器人程序，用于自动化车牌识别。
    * 使用 OpenCV 进行图像处理和车牌区域检测。
    * 使用 Tesseract OCR 识别车牌字符。
    * 从标准输入读取格式为rawvideo bgr24 640x480视频帧数据。
    * 检测到车牌后，通过 HTTP POST 请求将车牌号和动作 (入场/出场) 发送给服务器的 `/api/opencv/process` 接口。
    * 通过 `config_bot.json` 配置服务器连接信息、机器人 token 和角色 (入口/出口)。
    * 需要 `haarcascade_russian_plate_number.xml` 文件用于车牌检测。

## 主要功能

* **前后端**
    * 前后端分类，使用http+json通讯。
* **用户认证**:
    * 支持管理员 (`admin`) 和普通用户 (`user`) 角色。
    * 密码使用 SHA256 哈希存储。
    * 基于 Token 的会话管理。
    * 独立的 Bot Token 认证机制。
* **车辆管理**:
    * 记录车辆入场和出场时间。
    * 计算停车时长和费用（基于可配置的免费时长、计费周期、周期价格、每日封顶费用）。
    * 支持月卡车辆（续费、到期判断）。
    * 黑名单管理（添加、移除、禁止入场）。
    * 查询单个车辆的详细信息（是否在场、是否月卡、月卡到期时间、是否黑名单、历史进出记录等）。
    * 查询所有已记录车牌和当前在场车牌列表。
* **自动化**: (通过 `parking_system_bot`)
    * 基于 OpenCV 和 Tesseract 的车牌自动识别与上报。
* **日志记录**:
    * 记录详细的用户、车辆、管理员操作日志。

## 技术栈与依赖

* C++17
* CMake (>= 3.10)
* **库**:
    * [nlohmann/json](https://github.com/nlohmann/json) (v3.11.2): 处理 JSON 数据
    * [cpp-httplib](https://github.com/yhirose/cpp-httplib): HTTP 服务器/客户端库 (Server 使用)
    * libcurl: HTTP 客户端库 (Client 和 Bot 使用)
    * OpenSSL: 用于 SHA256 哈希计算
    * pthread: 多线程支持
* **`parking_system_bot` 额外依赖**:
    * OpenCV: 图像处理和车牌检测
    * Tesseract OCR: 车牌字符识别

## 构建

项目使用 CMake 进行构建。

```bash
# 1. 克隆仓库
git clone [https://github.com/CZB666-wdnmd/parking_system_c-_hw.git](https://github.com/CZB666-wdnmd/parking_system_c-_hw.git)
cd parking_system_c-_hw

# 2. 创建构建目录
mkdir build
cd build

# 3. 运行 CMake 配置
#    确保已安装所有依赖 (OpenSSL, libcurl, OpenCV, Tesseract)
sudo apt update
sudo apt install -y build-essential cmake libssl-dev libcurl4-openssl-dev libopencv-dev tesseract-ocr

cmake ..

# 4. 编译项目
cmake --build . --target package

```

编译成功后，会在构建目录 (或指定的安装目录) 下生成一个打包文件：`parking_system.zip`，解压后完成配置即可运行。

## 配置

在运行程序之前，需要创建和配置相应的 JSON 文件。

1.  **`config.json`** (服务器配置):
    * `ip`: 服务器监听的 IP 地址 (例如 "0.0.0.0")。
    * `port`: 服务器监听的端口号 (例如 8080)。
    * `freetime`: 免费停车分钟数。
    * `fee_stage_time`: 计费周期（分钟）。
    * `fee_stage_price`: 每个计费周期的价格。
    * `fee_day_top`: 每日最高收费。
    * *示例*:
      ```json
      {
          "ip": "0.0.0.0",
          "port": 8080,
          "freetime": 30,
          "fee_stage_time": 60,
          "fee_stage_price": 5.0,
          "fee_day_top": 50.0
      }
      ```

2.  **`users.json`** (用户数据库):
    * 需要手动创建此文件。
    * 包含一个用户对象的数组。
    * 每个用户对象包含 `username`, `role` ("admin", "user", 或 "bot"), `auth` (对于 admin/user 是密码的 SHA256 哈希，对于 bot 是预设的 token)。
    * *示例*:
      ```json
      [
          {
              "username": "admin",
              "role": "admin",
              "auth": "sha256_hash_of_admin_password"
          },
          {
              "username": "testuser",
              "role": "user",
              "auth": "sha256_hash_of_user_password"
          },
          {
              "username": "entry_bot",
              "role": "bot",
              "auth": "your_secret_entry_bot_token"
          },
          {
              "username": "exit_bot",
              "role": "bot",
              "auth": "your_secret_exit_bot_token"
          }
      ]
      ```

3.  **`vehicles.json`** (车辆数据库):
    * 如果不存在，服务器启动时会自动创建为空对象 `{}`。
    * 服务器运行时会自动更新此文件。

4.  **`config_client.json`** (客户端配置):
    * `ip`: 服务器的 IP 地址。
    * `port`: 服务器的端口号。
    * *示例*:
      ```json
      {
          "ip": "127.0.0.1",
          "port": 8080
      }
      ```

5.  **`config_bot.json`** (机器人配置):
    * `ip`: 服务器的 IP 地址。
    * `port`: 服务器的端口号。
    * `token`: 对应 `users.json` 中配置的 bot token。
    * `role`: "entry" 或 "exit"，指示此机器人是用于入口还是出口。
    * *示例*:
      ```json
      {
          "ip": "127.0.0.1",
          "port": 8080,
          "token": "your_secret_entry_bot_token",
          "role": "entry"
      }
      ```

6.  **`haarcascade_russian_plate_number.xml`** (Bot 使用):
    * 这是 OpenCV 用于车牌检测的级联分类器文件。已包含在打包完成的文件中。

## 运行

1.  **启动服务器**:
    ```bash
    ./parking_system_server
    ```
    确保 `config.json`, `users.json` 在同一目录下。`vehicles.json` 和 `system.log` 会自动创建/更新。

2.  **运行客户端**:
    ```bash
    ./parking_system_client
    ```
    确保 `config_client.json` 在同一目录下。按照提示登录并操作。

3.  **运行机器人**:
    ```bash
    ffmpeg 你的视频来源（文件或设备）-f rawvideo -pix_fmt bgr24 -s 640x480 | ./parking_system_bot
    ```
    确保 `config_bot.json` 和 `haarcascade_russian_plate_number.xml` 在同一目录下。机器人会从标准输入读取帧数据进行处理。
